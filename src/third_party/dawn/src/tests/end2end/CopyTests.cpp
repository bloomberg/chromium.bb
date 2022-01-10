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

#include "tests/DawnTest.h"

#include <array>
#include "common/Constants.h"
#include "common/Math.h"
#include "utils/TestUtils.h"
#include "utils/TextureUtils.h"
#include "utils/WGPUHelpers.h"

// For MinimumBufferSpec bytesPerRow and rowsPerImage, compute a default from the copy extent.
constexpr uint32_t kStrideComputeDefault = 0xFFFF'FFFEul;

constexpr wgpu::TextureFormat kDefaultFormat = wgpu::TextureFormat::RGBA8Unorm;

class CopyTests {
  protected:
    struct TextureSpec {
        wgpu::TextureFormat format = kDefaultFormat;
        wgpu::Origin3D copyOrigin = {0, 0, 0};
        wgpu::Extent3D textureSize;
        uint32_t copyLevel = 0;
        uint32_t levelCount = 1;
    };

    struct BufferSpec {
        uint64_t size;
        uint64_t offset;
        uint32_t bytesPerRow;
        uint32_t rowsPerImage;
    };

    static std::vector<uint8_t> GetExpectedTextureData(const utils::TextureDataCopyLayout& layout) {
        uint32_t bytesPerTexelBlock = layout.bytesPerRow / layout.texelBlocksPerRow;
        std::vector<uint8_t> textureData(layout.byteLength);
        for (uint32_t layer = 0; layer < layout.mipSize.depthOrArrayLayers; ++layer) {
            const uint32_t byteOffsetPerSlice = layout.bytesPerImage * layer;
            for (uint32_t y = 0; y < layout.mipSize.height; ++y) {
                for (uint32_t x = 0; x < layout.mipSize.width * bytesPerTexelBlock; ++x) {
                    uint32_t i = x + y * layout.bytesPerRow;
                    textureData[byteOffsetPerSlice + i] =
                        static_cast<uint8_t>((x + 1 + (layer + 1) * y) % 256);
                }
            }
        }
        return textureData;
    }

    // TODO(crbug.com/dawn/818): remove this function when all the tests in this file support
    // testing arbitrary formats.
    static std::vector<RGBA8> GetExpectedTextureDataRGBA8(
        const utils::TextureDataCopyLayout& layout) {
        std::vector<RGBA8> textureData(layout.texelBlockCount);
        for (uint32_t layer = 0; layer < layout.mipSize.depthOrArrayLayers; ++layer) {
            const uint32_t texelIndexOffsetPerSlice = layout.texelBlocksPerImage * layer;
            for (uint32_t y = 0; y < layout.mipSize.height; ++y) {
                for (uint32_t x = 0; x < layout.mipSize.width; ++x) {
                    uint32_t i = x + y * layout.texelBlocksPerRow;
                    textureData[texelIndexOffsetPerSlice + i] =
                        RGBA8(static_cast<uint8_t>((x + layer * x) % 256),
                              static_cast<uint8_t>((y + layer * y) % 256),
                              static_cast<uint8_t>(x / 256), static_cast<uint8_t>(y / 256));
                }
            }
        }

        return textureData;
    }

    static BufferSpec MinimumBufferSpec(uint32_t width,
                                        uint32_t height,
                                        uint32_t depth = 1,
                                        wgpu::TextureFormat format = kDefaultFormat) {
        return MinimumBufferSpec({width, height, depth}, kStrideComputeDefault,
                                 depth == 1 ? wgpu::kCopyStrideUndefined : kStrideComputeDefault,
                                 format);
    }

    static BufferSpec MinimumBufferSpec(wgpu::Extent3D copyExtent,
                                        uint32_t overrideBytesPerRow = kStrideComputeDefault,
                                        uint32_t overrideRowsPerImage = kStrideComputeDefault,
                                        wgpu::TextureFormat format = kDefaultFormat) {
        uint32_t bytesPerRow = utils::GetMinimumBytesPerRow(format, copyExtent.width);
        if (overrideBytesPerRow != kStrideComputeDefault) {
            bytesPerRow = overrideBytesPerRow;
        }
        uint32_t rowsPerImage = copyExtent.height;
        if (overrideRowsPerImage != kStrideComputeDefault) {
            rowsPerImage = overrideRowsPerImage;
        }

        uint32_t totalDataSize =
            utils::RequiredBytesInCopy(bytesPerRow, rowsPerImage, copyExtent, format);
        return {totalDataSize, 0, bytesPerRow, rowsPerImage};
    }
    static void CopyTextureData(uint32_t bytesPerTexelBlock,
                                const void* srcData,
                                uint32_t widthInBlocks,
                                uint32_t heightInBlocks,
                                uint32_t depthInBlocks,
                                uint32_t srcBytesPerRow,
                                uint32_t srcRowsPerImage,
                                void* dstData,
                                uint32_t dstBytesPerRow,
                                uint32_t dstRowsPerImage) {
        for (unsigned int z = 0; z < depthInBlocks; ++z) {
            uint32_t srcDepthOffset = z * srcBytesPerRow * srcRowsPerImage;
            uint32_t dstDepthOffset = z * dstBytesPerRow * dstRowsPerImage;
            for (unsigned int y = 0; y < heightInBlocks; ++y) {
                memcpy(static_cast<uint8_t*>(dstData) + dstDepthOffset + y * dstBytesPerRow,
                       static_cast<const uint8_t*>(srcData) + srcDepthOffset + y * srcBytesPerRow,
                       widthInBlocks * bytesPerTexelBlock);
            }
        }
    }
};

class CopyTests_T2B : public CopyTests, public DawnTest {
  protected:
    void DoTest(const TextureSpec& textureSpec,
                const BufferSpec& bufferSpec,
                const wgpu::Extent3D& copySize,
                wgpu::TextureDimension dimension = wgpu::TextureDimension::e2D) {
        // TODO(crbug.com/dawn/818): support testing arbitrary formats
        ASSERT_EQ(kDefaultFormat, textureSpec.format);

        const uint32_t bytesPerTexel = utils::GetTexelBlockSizeInBytes(textureSpec.format);
        // Create a texture that is `width` x `height` with (`level` + 1) mip levels.
        wgpu::TextureDescriptor descriptor;
        descriptor.dimension = dimension;
        descriptor.size = textureSpec.textureSize;
        descriptor.sampleCount = 1;
        descriptor.format = textureSpec.format;
        descriptor.mipLevelCount = textureSpec.levelCount;
        descriptor.usage = wgpu::TextureUsage::CopyDst | wgpu::TextureUsage::CopySrc;
        wgpu::Texture texture = device.CreateTexture(&descriptor);

        // Layout for initial data upload to texture.
        // Some parts of this result are also reused later.
        const utils::TextureDataCopyLayout copyLayout =
            utils::GetTextureDataCopyLayoutForTextureAtLevel(
                textureSpec.format, textureSpec.textureSize, textureSpec.copyLevel, dimension);

        // Initialize the source texture
        std::vector<RGBA8> textureArrayData = GetExpectedTextureDataRGBA8(copyLayout);
        {
            wgpu::ImageCopyTexture imageCopyTexture =
                utils::CreateImageCopyTexture(texture, textureSpec.copyLevel, {0, 0, 0});
            wgpu::TextureDataLayout textureDataLayout =
                utils::CreateTextureDataLayout(0, copyLayout.bytesPerRow, copyLayout.rowsPerImage);
            queue.WriteTexture(&imageCopyTexture, textureArrayData.data(), copyLayout.byteLength,
                               &textureDataLayout, &copyLayout.mipSize);
        }

        wgpu::CommandEncoder encoder = device.CreateCommandEncoder();

        // Create a buffer of `size` and populate it with empty data (0,0,0,0) Note:
        // Prepopulating the buffer with empty data ensures that there is not random data in the
        // expectation and helps ensure that the padding due to the bytes per row is not modified
        // by the copy.
        wgpu::BufferDescriptor bufferDesc;
        bufferDesc.size = bufferSpec.size;
        bufferDesc.usage = wgpu::BufferUsage::CopySrc | wgpu::BufferUsage::CopyDst;
        wgpu::Buffer buffer = device.CreateBuffer(&bufferDesc);

        {
            wgpu::ImageCopyTexture imageCopyTexture = utils::CreateImageCopyTexture(
                texture, textureSpec.copyLevel, textureSpec.copyOrigin);
            wgpu::ImageCopyBuffer imageCopyBuffer = utils::CreateImageCopyBuffer(
                buffer, bufferSpec.offset, bufferSpec.bytesPerRow, bufferSpec.rowsPerImage);
            encoder.CopyTextureToBuffer(&imageCopyTexture, &imageCopyBuffer, &copySize);
        }

        wgpu::CommandBuffer commands = encoder.Finish();
        queue.Submit(1, &commands);

        uint64_t bufferOffset = bufferSpec.offset;

        uint32_t copyLayer = copySize.depthOrArrayLayers;
        uint32_t copyDepth = 1;
        if (dimension == wgpu::TextureDimension::e3D) {
            copyLayer = 1;
            copyDepth = copySize.depthOrArrayLayers;
        }

        const wgpu::Extent3D copySizePerLayer = {copySize.width, copySize.height, copyDepth};
        // Texels in single layer.
        const uint32_t texelCountInCopyRegion = utils::GetTexelCountInCopyRegion(
            bufferSpec.bytesPerRow, bufferSpec.rowsPerImage, copySizePerLayer, textureSpec.format);
        const uint32_t maxArrayLayer = textureSpec.copyOrigin.z + copyLayer;
        std::vector<RGBA8> expected(texelCountInCopyRegion);
        for (uint32_t layer = textureSpec.copyOrigin.z; layer < maxArrayLayer; ++layer) {
            // Copy the data used to create the upload buffer in the specified copy region to have
            // the same format as the expected buffer data.
            std::fill(expected.begin(), expected.end(), RGBA8());
            const uint32_t texelIndexOffset = copyLayout.texelBlocksPerImage * layer;
            const uint32_t expectedTexelArrayDataStartIndex =
                texelIndexOffset + (textureSpec.copyOrigin.x +
                                    textureSpec.copyOrigin.y * copyLayout.texelBlocksPerRow);

            CopyTextureData(bytesPerTexel,
                            textureArrayData.data() + expectedTexelArrayDataStartIndex,
                            copySize.width, copySize.height, copyDepth, copyLayout.bytesPerRow,
                            copyLayout.rowsPerImage, expected.data(), bufferSpec.bytesPerRow,
                            bufferSpec.rowsPerImage);

            EXPECT_BUFFER_U32_RANGE_EQ(reinterpret_cast<const uint32_t*>(expected.data()), buffer,
                                       bufferOffset, static_cast<uint32_t>(expected.size()))
                << "Texture to Buffer copy failed copying region [(" << textureSpec.copyOrigin.x
                << ", " << textureSpec.copyOrigin.y << ", " << textureSpec.copyOrigin.z << "), ("
                << textureSpec.copyOrigin.x + copySize.width << ", "
                << textureSpec.copyOrigin.y + copySize.height << ", "
                << textureSpec.copyOrigin.z + copySize.depthOrArrayLayers << ")) from "
                << textureSpec.textureSize.width << " x " << textureSpec.textureSize.height
                << " texture at mip level " << textureSpec.copyLevel << " layer " << layer << " to "
                << bufferSpec.size << "-byte buffer with offset " << bufferOffset
                << " and bytes per row " << bufferSpec.bytesPerRow << std::endl;

            bufferOffset += bufferSpec.bytesPerRow * bufferSpec.rowsPerImage;
        }
    }
};

class CopyTests_B2T : public CopyTests, public DawnTest {
  protected:
    static void FillBufferData(RGBA8* data, size_t count) {
        for (size_t i = 0; i < count; ++i) {
            data[i] = RGBA8(static_cast<uint8_t>(i % 256), static_cast<uint8_t>((i / 256) % 256),
                            static_cast<uint8_t>((i / 256 / 256) % 256), 255);
        }
    }

    void DoTest(const TextureSpec& textureSpec,
                const BufferSpec& bufferSpec,
                const wgpu::Extent3D& copySize,
                wgpu::TextureDimension dimension = wgpu::TextureDimension::e2D) {
        // TODO(crbug.com/dawn/818): support testing arbitrary formats
        ASSERT_EQ(kDefaultFormat, textureSpec.format);
        // Create a buffer of size `size` and populate it with data
        const uint32_t bytesPerTexel = utils::GetTexelBlockSizeInBytes(textureSpec.format);
        std::vector<RGBA8> bufferData(bufferSpec.size / bytesPerTexel);
        FillBufferData(bufferData.data(), bufferData.size());
        wgpu::Buffer buffer =
            utils::CreateBufferFromData(device, bufferData.data(), bufferSpec.size,
                                        wgpu::BufferUsage::CopySrc | wgpu::BufferUsage::CopyDst);

        // Create a texture that is `width` x `height` with (`level` + 1) mip levels.
        wgpu::TextureDescriptor descriptor;
        descriptor.dimension = dimension;
        descriptor.size = textureSpec.textureSize;
        descriptor.sampleCount = 1;
        descriptor.format = textureSpec.format;
        descriptor.mipLevelCount = textureSpec.levelCount;
        descriptor.usage = wgpu::TextureUsage::CopyDst | wgpu::TextureUsage::CopySrc;
        wgpu::Texture texture = device.CreateTexture(&descriptor);

        wgpu::CommandEncoder encoder = device.CreateCommandEncoder();

        wgpu::ImageCopyBuffer imageCopyBuffer = utils::CreateImageCopyBuffer(
            buffer, bufferSpec.offset, bufferSpec.bytesPerRow, bufferSpec.rowsPerImage);
        wgpu::ImageCopyTexture imageCopyTexture =
            utils::CreateImageCopyTexture(texture, textureSpec.copyLevel, textureSpec.copyOrigin);
        encoder.CopyBufferToTexture(&imageCopyBuffer, &imageCopyTexture, &copySize);

        wgpu::CommandBuffer commands = encoder.Finish();
        queue.Submit(1, &commands);

        const utils::TextureDataCopyLayout copyLayout =
            utils::GetTextureDataCopyLayoutForTextureAtLevel(
                textureSpec.format, textureSpec.textureSize, textureSpec.copyLevel, dimension,
                bufferSpec.rowsPerImage);

        uint32_t copyLayer = copySize.depthOrArrayLayers;
        uint32_t copyDepth = 1;
        if (dimension == wgpu::TextureDimension::e3D) {
            copyLayer = 1;
            copyDepth = copySize.depthOrArrayLayers;
        }

        uint64_t bufferOffset = bufferSpec.offset;
        const uint32_t blockWidth = utils::GetTextureFormatBlockWidth(textureSpec.format);
        const uint32_t blockHeight = utils::GetTextureFormatBlockHeight(textureSpec.format);
        const uint32_t texelCountPerLayer = copyDepth * (copyLayout.mipSize.width / blockWidth) *
                                            (copyLayout.mipSize.height / blockHeight) *
                                            bytesPerTexel;
        for (uint32_t layer = 0; layer < copyLayer; ++layer) {
            // Copy and pack the data used to create the buffer in the specified copy region to have
            // the same format as the expected texture data.
            std::vector<RGBA8> expected(texelCountPerLayer);
            CopyTextureData(bytesPerTexel, bufferData.data() + bufferOffset / bytesPerTexel,
                            copySize.width, copySize.height, copyDepth, bufferSpec.bytesPerRow,
                            bufferSpec.rowsPerImage, expected.data(),
                            copySize.width * bytesPerTexel, copySize.height);

            EXPECT_TEXTURE_EQ(expected.data(), texture,
                              {textureSpec.copyOrigin.x, textureSpec.copyOrigin.y,
                               textureSpec.copyOrigin.z + layer},
                              {copySize.width, copySize.height, copyDepth}, textureSpec.copyLevel)
                << "Buffer to Texture copy failed copying " << bufferSpec.size
                << "-byte buffer with offset " << bufferSpec.offset << " and bytes per row "
                << bufferSpec.bytesPerRow << " to [(" << textureSpec.copyOrigin.x << ", "
                << textureSpec.copyOrigin.y << "), (" << textureSpec.copyOrigin.x + copySize.width
                << ", " << textureSpec.copyOrigin.y + copySize.height << ")) region of "
                << textureSpec.textureSize.width << " x " << textureSpec.textureSize.height
                << " texture at mip level " << textureSpec.copyLevel << " layer " << layer
                << std::endl;
            bufferOffset += copyLayout.bytesPerImage;
        }
    }
};

namespace {
    // The CopyTests Texture to Texture in this class will validate both CopyTextureToTexture and
    // CopyTextureToTextureInternal.
    using UsageCopySrc = bool;
    DAWN_TEST_PARAM_STRUCT(CopyTestsParams, UsageCopySrc);
}  // namespace

class CopyTests_T2T : public CopyTests, public DawnTestWithParams<CopyTestsParams> {
  protected:
    std::vector<const char*> GetRequiredFeatures() override {
        return {"dawn-internal-usages"};
    }

    void DoTest(const TextureSpec& srcSpec,
                const TextureSpec& dstSpec,
                const wgpu::Extent3D& copySize,
                bool copyWithinSameTexture = false,
                wgpu::TextureDimension dimension = wgpu::TextureDimension::e2D) {
        DoTest(srcSpec, dstSpec, copySize, dimension, dimension, copyWithinSameTexture);
    }

    void DoTest(const TextureSpec& srcSpec,
                const TextureSpec& dstSpec,
                const wgpu::Extent3D& copySize,
                wgpu::TextureDimension srcDimension,
                wgpu::TextureDimension dstDimension,
                bool copyWithinSameTexture = false) {
        const bool usageCopySrc = GetParam().mUsageCopySrc;
        // If we do this test with a CopyWithinSameTexture, it will need to have usageCopySrc in the
        // public usage of the texture as it will later use a CopyTextureToBuffer, that needs the
        // public usage of it.
        DAWN_TEST_UNSUPPORTED_IF(!usageCopySrc && copyWithinSameTexture);

        ASSERT_EQ(srcSpec.format, dstSpec.format);
        const wgpu::TextureFormat format = srcSpec.format;

        wgpu::TextureDescriptor srcDescriptor;
        srcDescriptor.dimension = srcDimension;
        srcDescriptor.size = srcSpec.textureSize;
        srcDescriptor.sampleCount = 1;
        srcDescriptor.format = format;
        srcDescriptor.mipLevelCount = srcSpec.levelCount;
        srcDescriptor.usage = wgpu::TextureUsage::CopyDst;
        // This test will have two versions, one where we check the normal CopyToCopy, and for that
        // we will add the CopySrc usage, and the one where we test the CopyToCopyInternal, and then
        // we have to add the CopySrc to the Internal usage.
        wgpu::DawnTextureInternalUsageDescriptor internalDesc = {};
        srcDescriptor.nextInChain = &internalDesc;
        if (usageCopySrc) {
            srcDescriptor.usage |= wgpu::TextureUsage::CopySrc;
        } else {
            internalDesc.internalUsage = wgpu::TextureUsage::CopySrc;
        }
        wgpu::Texture srcTexture = device.CreateTexture(&srcDescriptor);

        wgpu::Texture dstTexture;
        if (copyWithinSameTexture) {
            dstTexture = srcTexture;
        } else {
            wgpu::TextureDescriptor dstDescriptor;
            dstDescriptor.dimension = dstDimension;
            dstDescriptor.size = dstSpec.textureSize;
            dstDescriptor.sampleCount = 1;
            dstDescriptor.format = format;
            dstDescriptor.mipLevelCount = dstSpec.levelCount;
            dstDescriptor.usage = wgpu::TextureUsage::CopySrc | wgpu::TextureUsage::CopyDst;
            dstTexture = device.CreateTexture(&dstDescriptor);
        }

        // Create an upload buffer and use it to completely populate the subresources of the src
        // texture that will be copied from at the given mip level.
        const utils::TextureDataCopyLayout srcDataCopyLayout =
            utils::GetTextureDataCopyLayoutForTextureAtLevel(
                format,
                {srcSpec.textureSize.width, srcSpec.textureSize.height,
                 srcDimension == wgpu::TextureDimension::e3D
                     ? srcSpec.textureSize.depthOrArrayLayers
                     : copySize.depthOrArrayLayers},
                srcSpec.copyLevel, srcDimension);

        // Initialize the source texture
        const std::vector<uint8_t> srcTextureCopyData = GetExpectedTextureData(srcDataCopyLayout);
        {
            wgpu::ImageCopyTexture imageCopyTexture = utils::CreateImageCopyTexture(
                srcTexture, srcSpec.copyLevel, {0, 0, srcSpec.copyOrigin.z});
            wgpu::TextureDataLayout textureDataLayout = utils::CreateTextureDataLayout(
                0, srcDataCopyLayout.bytesPerRow, srcDataCopyLayout.rowsPerImage);
            queue.WriteTexture(&imageCopyTexture, srcTextureCopyData.data(),
                               srcDataCopyLayout.byteLength, &textureDataLayout,
                               &srcDataCopyLayout.mipSize);
        }

        wgpu::CommandEncoder encoder = device.CreateCommandEncoder();

        // Perform the texture to texture copy
        wgpu::ImageCopyTexture srcImageCopyTexture =
            utils::CreateImageCopyTexture(srcTexture, srcSpec.copyLevel, srcSpec.copyOrigin);
        wgpu::ImageCopyTexture dstImageCopyTexture =
            utils::CreateImageCopyTexture(dstTexture, dstSpec.copyLevel, dstSpec.copyOrigin);

        if (usageCopySrc) {
            encoder.CopyTextureToTexture(&srcImageCopyTexture, &dstImageCopyTexture, &copySize);
        } else {
            encoder.CopyTextureToTextureInternal(&srcImageCopyTexture, &dstImageCopyTexture,
                                                 &copySize);
        }

        // Create an output buffer and use it to completely populate the subresources of the dst
        // texture that will be copied to at the given mip level.
        const utils::TextureDataCopyLayout dstDataCopyLayout =
            utils::GetTextureDataCopyLayoutForTextureAtLevel(
                format,
                {dstSpec.textureSize.width, dstSpec.textureSize.height,
                 dstDimension == wgpu::TextureDimension::e3D
                     ? dstSpec.textureSize.depthOrArrayLayers
                     : copySize.depthOrArrayLayers},
                dstSpec.copyLevel, dstDimension);
        wgpu::BufferDescriptor outputBufferDescriptor;
        outputBufferDescriptor.size = dstDataCopyLayout.byteLength;
        outputBufferDescriptor.usage = wgpu::BufferUsage::CopySrc | wgpu::BufferUsage::CopyDst;
        wgpu::Buffer outputBuffer = device.CreateBuffer(&outputBufferDescriptor);
        const uint32_t bytesPerTexel = utils::GetTexelBlockSizeInBytes(format);
        const uint32_t expectedDstDataOffset = dstSpec.copyOrigin.x * bytesPerTexel +
                                               dstSpec.copyOrigin.y * dstDataCopyLayout.bytesPerRow;
        wgpu::ImageCopyBuffer outputImageCopyBuffer = utils::CreateImageCopyBuffer(
            outputBuffer, expectedDstDataOffset, dstDataCopyLayout.bytesPerRow,
            dstDataCopyLayout.rowsPerImage);
        encoder.CopyTextureToBuffer(&dstImageCopyTexture, &outputImageCopyBuffer, &copySize);

        wgpu::CommandBuffer commands = encoder.Finish();
        queue.Submit(1, &commands);

        // Validate if the data in outputBuffer is what we expected, including the untouched data
        // outside of the copy.
        {
            // Validate the output buffer slice-by-slice regardless of whether the destination
            // texture is 3D or 2D. The dimension here doesn't matter - we're only populating the
            // CPU data to verify against.
            uint32_t copyLayer = copySize.depthOrArrayLayers;
            uint32_t copyDepth = 1;

            const uint64_t validDataSizePerDstTextureLayer = utils::RequiredBytesInCopy(
                dstDataCopyLayout.bytesPerRow, dstDataCopyLayout.mipSize.height,
                dstDataCopyLayout.mipSize.width, dstDataCopyLayout.mipSize.height, copyDepth,
                bytesPerTexel);

            // expectedDstDataPerSlice stores one layer of the destination texture.
            std::vector<uint8_t> expectedDstDataPerSlice(validDataSizePerDstTextureLayer);
            for (uint32_t slice = 0; slice < copyLayer; ++slice) {
                // For each source texture array slice involved in the copy, emulate the T2T copy
                // on the CPU side by "copying" the copy data from the "source texture"
                // (srcTextureCopyData) to the "destination texture" (expectedDstDataPerSlice).
                std::fill(expectedDstDataPerSlice.begin(), expectedDstDataPerSlice.end(), 0);

                const uint32_t srcBytesOffset = srcDataCopyLayout.bytesPerImage * slice;

                // Get the offset of the srcTextureCopyData that contains the copy data on the
                // slice-th texture array layer of the source texture.
                const uint32_t srcTexelDataOffset =
                    srcBytesOffset + (srcSpec.copyOrigin.x * bytesPerTexel +
                                      srcSpec.copyOrigin.y * srcDataCopyLayout.bytesPerRow);
                // Do the T2T "copy" on the CPU side to get the expected texel value at the
                CopyTextureData(bytesPerTexel, &srcTextureCopyData[srcTexelDataOffset],
                                copySize.width, copySize.height, copyDepth,
                                srcDataCopyLayout.bytesPerRow, srcDataCopyLayout.rowsPerImage,
                                &expectedDstDataPerSlice[expectedDstDataOffset],
                                dstDataCopyLayout.bytesPerRow, dstDataCopyLayout.rowsPerImage);

                // Compare the content of the destination texture at the (dstSpec.copyOrigin.z +
                // slice)-th layer to its expected data after the copy (the outputBuffer contains
                // the data of the destination texture since the dstSpec.copyOrigin.z-th layer).
                uint64_t outputBufferExpectationBytesOffset =
                    dstDataCopyLayout.bytesPerImage * slice;
                EXPECT_BUFFER_U32_RANGE_EQ(
                    reinterpret_cast<const uint32_t*>(expectedDstDataPerSlice.data()), outputBuffer,
                    outputBufferExpectationBytesOffset,
                    validDataSizePerDstTextureLayer / sizeof(uint32_t));
            }
        }
    }
};

class CopyTests_B2B : public DawnTest {
  protected:
    // This is the same signature as CopyBufferToBuffer except that the buffers are replaced by
    // only their size.
    void DoTest(uint64_t sourceSize,
                uint64_t sourceOffset,
                uint64_t destinationSize,
                uint64_t destinationOffset,
                uint64_t copySize) {
        ASSERT(sourceSize % 4 == 0);
        ASSERT(destinationSize % 4 == 0);

        // Create our two test buffers, destination filled with zeros, source filled with non-zeroes
        std::vector<uint32_t> zeroes(static_cast<size_t>(destinationSize / sizeof(uint32_t)));
        wgpu::Buffer destination =
            utils::CreateBufferFromData(device, zeroes.data(), zeroes.size() * sizeof(uint32_t),
                                        wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::CopySrc);

        std::vector<uint32_t> sourceData(static_cast<size_t>(sourceSize / sizeof(uint32_t)));
        for (size_t i = 0; i < sourceData.size(); i++) {
            sourceData[i] = i + 1;
        }
        wgpu::Buffer source = utils::CreateBufferFromData(device, sourceData.data(),
                                                          sourceData.size() * sizeof(uint32_t),
                                                          wgpu::BufferUsage::CopySrc);

        // Submit the copy
        wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
        encoder.CopyBufferToBuffer(source, sourceOffset, destination, destinationOffset, copySize);
        wgpu::CommandBuffer commands = encoder.Finish();
        queue.Submit(1, &commands);

        // Check destination is exactly the expected content.
        EXPECT_BUFFER_U32_RANGE_EQ(zeroes.data(), destination, 0,
                                   destinationOffset / sizeof(uint32_t));
        EXPECT_BUFFER_U32_RANGE_EQ(sourceData.data() + sourceOffset / sizeof(uint32_t), destination,
                                   destinationOffset, copySize / sizeof(uint32_t));
        uint64_t copyEnd = destinationOffset + copySize;
        EXPECT_BUFFER_U32_RANGE_EQ(zeroes.data(), destination, copyEnd,
                                   (destinationSize - copyEnd) / sizeof(uint32_t));
    }
};

class ClearBufferTests : public DawnTest {
  protected:
    // This is the same signature as ClearBuffer except that the buffers are replaced by
    // only their size.
    void DoTest(uint64_t bufferSize, uint64_t clearOffset, uint64_t clearSize) {
        ASSERT(bufferSize % 4 == 0);
        ASSERT(clearSize % 4 == 0);

        // Create our test buffer, filled with non-zeroes
        std::vector<uint32_t> bufferData(static_cast<size_t>(bufferSize / sizeof(uint32_t)));
        for (size_t i = 0; i < bufferData.size(); i++) {
            bufferData[i] = i + 1;
        }
        wgpu::Buffer buffer = utils::CreateBufferFromData(
            device, bufferData.data(), bufferData.size() * sizeof(uint32_t),
            wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::CopySrc);

        std::vector<uint8_t> fillData(static_cast<size_t>(clearSize), 0u);

        // Submit the fill
        wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
        encoder.ClearBuffer(buffer, clearOffset, clearSize);
        wgpu::CommandBuffer commands = encoder.Finish();
        queue.Submit(1, &commands);

        // Check destination is exactly the expected content.
        EXPECT_BUFFER_U32_RANGE_EQ(bufferData.data(), buffer, 0, clearOffset / sizeof(uint32_t));
        EXPECT_BUFFER_U8_RANGE_EQ(fillData.data(), buffer, clearOffset, clearSize);
        uint64_t clearEnd = clearOffset + clearSize;
        EXPECT_BUFFER_U32_RANGE_EQ(bufferData.data() + clearEnd / sizeof(uint32_t), buffer,
                                   clearEnd, (bufferSize - clearEnd) / sizeof(uint32_t));
    }
};

// Test that copying an entire texture with 256-byte aligned dimensions works
TEST_P(CopyTests_T2B, FullTextureAligned) {
    constexpr uint32_t kWidth = 256;
    constexpr uint32_t kHeight = 128;

    TextureSpec textureSpec;
    textureSpec.textureSize = {kWidth, kHeight, 1};

    DoTest(textureSpec, MinimumBufferSpec(kWidth, kHeight), {kWidth, kHeight, 1});
}

// Test noop copies
TEST_P(CopyTests_T2B, ZeroSizedCopy) {
    constexpr uint32_t kWidth = 256;
    constexpr uint32_t kHeight = 128;

    TextureSpec textureSpec;
    textureSpec.textureSize = {kWidth, kHeight, 1};

    DoTest(textureSpec, MinimumBufferSpec(kWidth, kHeight), {0, kHeight, 1});
    DoTest(textureSpec, MinimumBufferSpec(kWidth, kHeight), {kWidth, 0, 1});
    DoTest(textureSpec, MinimumBufferSpec(kWidth, kHeight), {kWidth, kHeight, 0});
}

// Test that copying an entire texture without 256-byte aligned dimensions works
TEST_P(CopyTests_T2B, FullTextureUnaligned) {
    constexpr uint32_t kWidth = 259;
    constexpr uint32_t kHeight = 127;

    TextureSpec textureSpec;
    textureSpec.textureSize = {kWidth, kHeight, 1};

    DoTest(textureSpec, MinimumBufferSpec(kWidth, kHeight), {kWidth, kHeight, 1});
}

// Test that reading pixels from a 256-byte aligned texture works
TEST_P(CopyTests_T2B, PixelReadAligned) {
    constexpr uint32_t kWidth = 256;
    constexpr uint32_t kHeight = 128;
    BufferSpec pixelBuffer = MinimumBufferSpec(1, 1);

    constexpr wgpu::Extent3D kCopySize = {1, 1, 1};
    constexpr wgpu::Extent3D kTextureSize = {kWidth, kHeight, 1};
    TextureSpec defaultTextureSpec;
    defaultTextureSpec.textureSize = kTextureSize;

    {
        TextureSpec textureSpec = defaultTextureSpec;
        DoTest(textureSpec, pixelBuffer, kCopySize);
    }

    {
        TextureSpec textureSpec = defaultTextureSpec;
        textureSpec.copyOrigin = {kWidth - 1, 0, 0};
        DoTest(textureSpec, pixelBuffer, kCopySize);
    }

    {
        TextureSpec textureSpec = defaultTextureSpec;
        textureSpec.copyOrigin = {0, kHeight - 1, 0};
        DoTest(textureSpec, pixelBuffer, kCopySize);
    }

    {
        TextureSpec textureSpec = defaultTextureSpec;
        textureSpec.copyOrigin = {kWidth - 1, kHeight - 1, 0};
        DoTest(textureSpec, pixelBuffer, kCopySize);
    }

    {
        TextureSpec textureSpec = defaultTextureSpec;
        textureSpec.copyOrigin = {kWidth / 3, kHeight / 7, 0};
        DoTest(textureSpec, pixelBuffer, kCopySize);
    }

    {
        TextureSpec textureSpec = defaultTextureSpec;
        textureSpec.copyOrigin = {kWidth / 7, kHeight / 3, 0};
        DoTest(textureSpec, pixelBuffer, kCopySize);
    }
}

// Test that copying pixels from a texture that is not 256-byte aligned works
TEST_P(CopyTests_T2B, PixelReadUnaligned) {
    constexpr uint32_t kWidth = 259;
    constexpr uint32_t kHeight = 127;
    BufferSpec pixelBuffer = MinimumBufferSpec(1, 1);

    constexpr wgpu::Extent3D kCopySize = {1, 1, 1};
    constexpr wgpu::Extent3D kTextureSize = {kWidth, kHeight, 1};
    TextureSpec defaultTextureSpec;
    defaultTextureSpec.textureSize = kTextureSize;

    {
        TextureSpec textureSpec = defaultTextureSpec;
        DoTest(textureSpec, pixelBuffer, kCopySize);
    }

    {
        TextureSpec textureSpec = defaultTextureSpec;
        textureSpec.copyOrigin = {kWidth - 1, 0, 0};
        DoTest(textureSpec, pixelBuffer, kCopySize);
    }

    {
        TextureSpec textureSpec = defaultTextureSpec;
        textureSpec.copyOrigin = {0, kHeight - 1, 0};
        DoTest(textureSpec, pixelBuffer, kCopySize);
    }

    {
        TextureSpec textureSpec = defaultTextureSpec;
        textureSpec.copyOrigin = {kWidth - 1, kHeight - 1, 0};
        DoTest(textureSpec, pixelBuffer, kCopySize);
    }

    {
        TextureSpec textureSpec = defaultTextureSpec;
        textureSpec.copyOrigin = {kWidth / 3, kHeight / 7, 0};
        DoTest(textureSpec, pixelBuffer, kCopySize);
    }

    {
        TextureSpec textureSpec = defaultTextureSpec;
        textureSpec.copyOrigin = {kWidth / 7, kHeight / 3, 0};
        DoTest(textureSpec, pixelBuffer, kCopySize);
    }
}

// Test that copying regions with 256-byte aligned sizes works
TEST_P(CopyTests_T2B, TextureRegionAligned) {
    constexpr uint32_t kWidth = 256;
    constexpr uint32_t kHeight = 128;
    for (unsigned int w : {64, 128, 256}) {
        for (unsigned int h : {16, 32, 48}) {
            TextureSpec textureSpec;
            textureSpec.textureSize = {kWidth, kHeight, 1};
            DoTest(textureSpec, MinimumBufferSpec(w, h), {w, h, 1});
        }
    }
}

// Test that copying regions without 256-byte aligned sizes works
TEST_P(CopyTests_T2B, TextureRegionUnaligned) {
    constexpr uint32_t kWidth = 256;
    constexpr uint32_t kHeight = 128;

    TextureSpec defaultTextureSpec;
    defaultTextureSpec.textureSize = {kWidth, kHeight, 1};

    for (unsigned int w : {13, 63, 65}) {
        for (unsigned int h : {17, 19, 63}) {
            TextureSpec textureSpec = defaultTextureSpec;
            DoTest(textureSpec, MinimumBufferSpec(w, h), {w, h, 1});
        }
    }
}

// Test that copying mips with 256-byte aligned sizes works
TEST_P(CopyTests_T2B, TextureMipAligned) {
    constexpr uint32_t kWidth = 256;
    constexpr uint32_t kHeight = 128;

    TextureSpec defaultTextureSpec;
    defaultTextureSpec.textureSize = {kWidth, kHeight, 1};

    for (unsigned int i = 1; i < 4; ++i) {
        TextureSpec textureSpec = defaultTextureSpec;
        textureSpec.copyLevel = i;
        textureSpec.levelCount = i + 1;
        DoTest(textureSpec, MinimumBufferSpec(kWidth >> i, kHeight >> i),
               {kWidth >> i, kHeight >> i, 1});
    }
}

// Test that copying mips when one dimension is 256-byte aligned and another dimension reach one
// works
TEST_P(CopyTests_T2B, TextureMipDimensionReachOne) {
    constexpr uint32_t mipLevelCount = 4;
    constexpr uint32_t kWidth = 256 << mipLevelCount;
    constexpr uint32_t kHeight = 2;

    TextureSpec defaultTextureSpec;
    defaultTextureSpec.textureSize = {kWidth, kHeight, 1};

    TextureSpec textureSpec = defaultTextureSpec;
    textureSpec.levelCount = mipLevelCount;

    for (unsigned int i = 0; i < 4; ++i) {
        textureSpec.copyLevel = i;
        DoTest(textureSpec,
               MinimumBufferSpec(std::max(kWidth >> i, 1u), std::max(kHeight >> i, 1u)),
               {std::max(kWidth >> i, 1u), std::max(kHeight >> i, 1u), 1});
    }
}

// Test that copying mips without 256-byte aligned sizes works
TEST_P(CopyTests_T2B, TextureMipUnaligned) {
    constexpr uint32_t kWidth = 259;
    constexpr uint32_t kHeight = 127;

    TextureSpec defaultTextureSpec;
    defaultTextureSpec.textureSize = {kWidth, kHeight, 1};

    for (unsigned int i = 1; i < 4; ++i) {
        TextureSpec textureSpec = defaultTextureSpec;
        textureSpec.copyLevel = i;
        textureSpec.levelCount = i + 1;
        DoTest(textureSpec, MinimumBufferSpec(kWidth >> i, kHeight >> i),
               {kWidth >> i, kHeight >> i, 1});
    }
}

// Test that copying with a 512-byte aligned buffer offset works
TEST_P(CopyTests_T2B, OffsetBufferAligned) {
    constexpr uint32_t kWidth = 256;
    constexpr uint32_t kHeight = 128;

    TextureSpec textureSpec;
    textureSpec.textureSize = {kWidth, kHeight, 1};

    for (unsigned int i = 0; i < 3; ++i) {
        BufferSpec bufferSpec = MinimumBufferSpec(kWidth, kHeight);
        uint64_t offset = 512 * i;
        bufferSpec.size += offset;
        bufferSpec.offset += offset;
        DoTest(textureSpec, bufferSpec, {kWidth, kHeight, 1});
    }
}

// Test that copying without a 512-byte aligned buffer offset works
TEST_P(CopyTests_T2B, OffsetBufferUnaligned) {
    constexpr uint32_t kWidth = 128;
    constexpr uint32_t kHeight = 128;

    TextureSpec textureSpec;
    textureSpec.textureSize = {kWidth, kHeight, 1};

    const uint32_t bytesPerTexel = utils::GetTexelBlockSizeInBytes(textureSpec.format);
    for (uint32_t i = bytesPerTexel; i < 512; i += bytesPerTexel * 9) {
        BufferSpec bufferSpec = MinimumBufferSpec(kWidth, kHeight);
        bufferSpec.size += i;
        bufferSpec.offset += i;
        DoTest(textureSpec, bufferSpec, {kWidth, kHeight, 1});
    }
}

// Test that copying without a 512-byte aligned buffer offset that is greater than the bytes per row
// works
TEST_P(CopyTests_T2B, OffsetBufferUnalignedSmallBytesPerRow) {
    constexpr uint32_t kWidth = 32;
    constexpr uint32_t kHeight = 128;

    TextureSpec textureSpec;
    textureSpec.textureSize = {kWidth, kHeight, 1};

    const uint32_t bytesPerTexel = utils::GetTexelBlockSizeInBytes(textureSpec.format);
    for (uint32_t i = 256 + bytesPerTexel; i < 512; i += bytesPerTexel * 9) {
        BufferSpec bufferSpec = MinimumBufferSpec(kWidth, kHeight);
        bufferSpec.size += i;
        bufferSpec.offset += i;
        DoTest(textureSpec, bufferSpec, {kWidth, kHeight, 1});
    }
}

// Test that copying with a greater bytes per row than needed on a 256-byte aligned texture works
TEST_P(CopyTests_T2B, BytesPerRowAligned) {
    constexpr uint32_t kWidth = 256;
    constexpr uint32_t kHeight = 128;

    TextureSpec textureSpec;
    textureSpec.textureSize = {kWidth, kHeight, 1};

    BufferSpec bufferSpec = MinimumBufferSpec(kWidth, kHeight);
    for (unsigned int i = 1; i < 4; ++i) {
        bufferSpec.bytesPerRow += 256;
        bufferSpec.size += 256 * kHeight;
        DoTest(textureSpec, bufferSpec, {kWidth, kHeight, 1});
    }
}

// Test that copying with a greater bytes per row than needed on a texture that is not 256-byte
// aligned works
TEST_P(CopyTests_T2B, BytesPerRowUnaligned) {
    constexpr uint32_t kWidth = 259;
    constexpr uint32_t kHeight = 127;

    TextureSpec textureSpec;
    textureSpec.textureSize = {kWidth, kHeight, 1};

    BufferSpec bufferSpec = MinimumBufferSpec(kWidth, kHeight);
    for (unsigned int i = 1; i < 4; ++i) {
        bufferSpec.bytesPerRow += 256;
        bufferSpec.size += 256 * kHeight;
        DoTest(textureSpec, bufferSpec, {kWidth, kHeight, 1});
    }
}

// Test that copying with bytesPerRow = 0 and bytesPerRow < bytesInACompleteRow works
// when we're copying one row only
TEST_P(CopyTests_T2B, BytesPerRowWithOneRowCopy) {
    constexpr uint32_t kWidth = 259;
    constexpr uint32_t kHeight = 127;

    TextureSpec textureSpec;
    textureSpec.textureSize = {kWidth, kHeight, 1};

    {
        BufferSpec bufferSpec = MinimumBufferSpec(5, 1);

        // bytesPerRow undefined
        bufferSpec.bytesPerRow = wgpu::kCopyStrideUndefined;
        DoTest(textureSpec, bufferSpec, {5, 1, 1});
    }
}

TEST_P(CopyTests_T2B, StrideSpecialCases) {
    TextureSpec textureSpec;
    textureSpec.textureSize = {4, 4, 4};

    // bytesPerRow 0
    for (const wgpu::Extent3D copyExtent :
         {wgpu::Extent3D{0, 2, 2}, {0, 0, 2}, {0, 2, 0}, {0, 0, 0}}) {
        DoTest(textureSpec, MinimumBufferSpec(copyExtent, 0, 2), copyExtent);
    }

    // bytesPerRow undefined
    for (const wgpu::Extent3D copyExtent :
         {wgpu::Extent3D{2, 1, 1}, {2, 0, 1}, {2, 1, 0}, {2, 0, 0}}) {
        DoTest(textureSpec, MinimumBufferSpec(copyExtent, wgpu::kCopyStrideUndefined, 2),
               copyExtent);
    }

    // rowsPerImage 0
    for (const wgpu::Extent3D copyExtent :
         {wgpu::Extent3D{2, 0, 2}, {2, 0, 0}, {0, 0, 2}, {0, 0, 0}}) {
        DoTest(textureSpec, MinimumBufferSpec(copyExtent, 256, 0), copyExtent);
    }

    // rowsPerImage undefined
    for (const wgpu::Extent3D copyExtent : {wgpu::Extent3D{2, 2, 1}, {2, 2, 0}}) {
        DoTest(textureSpec, MinimumBufferSpec(copyExtent, 256, wgpu::kCopyStrideUndefined),
               copyExtent);
    }
}

// Test copying a single slice with rowsPerImage larger than copy height and rowsPerImage will not
// take effect. If rowsPerImage takes effect, it looks like the copy may go past the end of the
// buffer.
TEST_P(CopyTests_T2B, RowsPerImageShouldNotCauseBufferOOBIfDepthOrArrayLayersIsOne) {
    // Check various offsets to cover each code path in the 2D split code in TextureCopySplitter.
    for (uint32_t offset : {0, 4, 64}) {
        constexpr uint32_t kWidth = 250;
        constexpr uint32_t kHeight = 3;

        TextureSpec textureSpec;
        textureSpec.textureSize = {kWidth, kHeight, 1};

        BufferSpec bufferSpec = MinimumBufferSpec(kWidth, kHeight);
        bufferSpec.rowsPerImage = 2 * kHeight;
        bufferSpec.offset = offset;
        bufferSpec.size += offset;
        DoTest(textureSpec, bufferSpec, {kWidth, kHeight, 1});
        DoTest(textureSpec, bufferSpec, {kWidth, kHeight, 1}, wgpu::TextureDimension::e3D);
    }
}

// Test copying a single row with bytesPerRow larger than copy width and bytesPerRow will not
// take effect. If bytesPerRow takes effect, it looks like the copy may go past the end of the
// buffer.
TEST_P(CopyTests_T2B, BytesPerRowShouldNotCauseBufferOOBIfCopyHeightIsOne) {
    // Check various offsets to cover each code path in the 2D split code in TextureCopySplitter.
    for (uint32_t offset : {0, 4, 100}) {
        constexpr uint32_t kWidth = 250;

        TextureSpec textureSpec;
        textureSpec.textureSize = {kWidth, 1, 1};

        BufferSpec bufferSpec = MinimumBufferSpec(kWidth, 1);
        bufferSpec.bytesPerRow = 1280;  // the default bytesPerRow is 1024.
        bufferSpec.offset = offset;
        bufferSpec.size += offset;
        DoTest(textureSpec, bufferSpec, {kWidth, 1, 1});
        DoTest(textureSpec, bufferSpec, {kWidth, 1, 1}, wgpu::TextureDimension::e3D);
    }
}

// A regression test for a bug on D3D12 backend that causes crash when doing texture-to-texture
// copy one row with the texture format Depth32Float.
TEST_P(CopyTests_T2B, CopyOneRowWithDepth32Float) {
    // TODO(crbug.com/dawn/727): currently this test fails on many D3D12 drivers.
    DAWN_SUPPRESS_TEST_IF(IsD3D12());

    constexpr wgpu::TextureFormat kFormat = wgpu::TextureFormat::Depth32Float;
    constexpr uint32_t kPixelsPerRow = 4u;

    wgpu::TextureDescriptor textureDescriptor;
    textureDescriptor.format = kFormat;
    textureDescriptor.size = {kPixelsPerRow, 1, 1};
    textureDescriptor.usage = wgpu::TextureUsage::CopySrc | wgpu::TextureUsage::RenderAttachment;
    wgpu::Texture texture = device.CreateTexture(&textureDescriptor);

    wgpu::CommandEncoder encoder = device.CreateCommandEncoder();

    // Initialize the depth texture with 0.5f.
    constexpr float kClearDepthValue = 0.5f;
    utils::ComboRenderPassDescriptor renderPass({}, texture.CreateView());
    renderPass.cDepthStencilAttachmentInfo.clearDepth = kClearDepthValue;
    renderPass.cDepthStencilAttachmentInfo.depthLoadOp = wgpu::LoadOp::Clear;
    renderPass.cDepthStencilAttachmentInfo.depthStoreOp = wgpu::StoreOp::Store;
    wgpu::RenderPassEncoder renderPassEncoder = encoder.BeginRenderPass(&renderPass);
    renderPassEncoder.EndPass();

    constexpr uint32_t kBufferCopyOffset = kTextureBytesPerRowAlignment;
    const uint32_t kBufferSize =
        kBufferCopyOffset + utils::GetTexelBlockSizeInBytes(kFormat) * kPixelsPerRow;
    wgpu::BufferDescriptor bufferDescriptor;
    bufferDescriptor.size = kBufferSize;
    bufferDescriptor.usage = wgpu::BufferUsage::CopySrc | wgpu::BufferUsage::CopyDst;
    wgpu::Buffer buffer = device.CreateBuffer(&bufferDescriptor);

    wgpu::ImageCopyBuffer imageCopyBuffer =
        utils::CreateImageCopyBuffer(buffer, kBufferCopyOffset, kTextureBytesPerRowAlignment);
    wgpu::ImageCopyTexture imageCopyTexture = utils::CreateImageCopyTexture(texture, 0, {0, 0, 0});

    wgpu::Extent3D copySize = textureDescriptor.size;
    encoder.CopyTextureToBuffer(&imageCopyTexture, &imageCopyBuffer, &copySize);
    wgpu::CommandBuffer commandBuffer = encoder.Finish();
    queue.Submit(1, &commandBuffer);

    std::array<float, kPixelsPerRow> expectedValues;
    std::fill(expectedValues.begin(), expectedValues.end(), kClearDepthValue);
    EXPECT_BUFFER_FLOAT_RANGE_EQ(expectedValues.data(), buffer, kBufferCopyOffset, kPixelsPerRow);
}

// Test that copying whole texture 2D array layers in one texture-to-buffer-copy works.
TEST_P(CopyTests_T2B, Texture2DArrayFull) {
    constexpr uint32_t kWidth = 256;
    constexpr uint32_t kHeight = 128;
    constexpr uint32_t kLayers = 6u;

    TextureSpec textureSpec;
    textureSpec.textureSize = {kWidth, kHeight, kLayers};

    DoTest(textureSpec, MinimumBufferSpec(kWidth, kHeight, kLayers), {kWidth, kHeight, kLayers});
}

// Test that copying a range of texture 2D array layers in one texture-to-buffer-copy works.
TEST_P(CopyTests_T2B, Texture2DArraySubRegion) {
    constexpr uint32_t kWidth = 256;
    constexpr uint32_t kHeight = 128;
    constexpr uint32_t kLayers = 6u;
    constexpr uint32_t kBaseLayer = 2u;
    constexpr uint32_t kCopyLayers = 3u;

    TextureSpec textureSpec;
    textureSpec.copyOrigin = {0, 0, kBaseLayer};
    textureSpec.textureSize = {kWidth, kHeight, kLayers};

    DoTest(textureSpec, MinimumBufferSpec(kWidth, kHeight, kCopyLayers),
           {kWidth, kHeight, kCopyLayers});
}

// Test that copying texture 2D array mips with 256-byte aligned sizes works
TEST_P(CopyTests_T2B, Texture2DArrayMip) {
    constexpr uint32_t kWidth = 256;
    constexpr uint32_t kHeight = 128;
    constexpr uint32_t kLayers = 6u;

    TextureSpec defaultTextureSpec;
    defaultTextureSpec.textureSize = {kWidth, kHeight, kLayers};

    for (unsigned int i = 1; i < 4; ++i) {
        TextureSpec textureSpec = defaultTextureSpec;
        textureSpec.copyLevel = i;
        textureSpec.levelCount = i + 1;

        DoTest(textureSpec, MinimumBufferSpec(kWidth >> i, kHeight >> i, kLayers),
               {kWidth >> i, kHeight >> i, kLayers});
    }
}

// Test that copying from a range of texture 2D array layers in one texture-to-buffer-copy when
// RowsPerImage is not equal to the height of the texture works.
TEST_P(CopyTests_T2B, Texture2DArrayRegionNonzeroRowsPerImage) {
    constexpr uint32_t kWidth = 256;
    constexpr uint32_t kHeight = 128;
    constexpr uint32_t kLayers = 6u;
    constexpr uint32_t kBaseLayer = 2u;
    constexpr uint32_t kCopyLayers = 3u;

    constexpr uint32_t kRowsPerImage = kHeight * 2;

    TextureSpec textureSpec;
    textureSpec.copyOrigin = {0, 0, kBaseLayer};
    textureSpec.textureSize = {kWidth, kHeight, kLayers};

    BufferSpec bufferSpec = MinimumBufferSpec(kWidth, kRowsPerImage, kCopyLayers);
    bufferSpec.rowsPerImage = kRowsPerImage;
    DoTest(textureSpec, bufferSpec, {kWidth, kHeight, kCopyLayers});
}

// Test a special code path in the D3D12 backends when (BytesPerRow * RowsPerImage) is not a
// multiple of 512.
TEST_P(CopyTests_T2B, Texture2DArrayRegionWithOffsetOddRowsPerImage) {
    constexpr uint32_t kWidth = 64;
    constexpr uint32_t kHeight = 128;
    constexpr uint32_t kLayers = 8u;
    constexpr uint32_t kBaseLayer = 2u;
    constexpr uint32_t kCopyLayers = 5u;

    constexpr uint32_t kRowsPerImage = kHeight + 1;

    TextureSpec textureSpec;
    textureSpec.copyOrigin = {0, 0, kBaseLayer};
    textureSpec.textureSize = {kWidth, kHeight, kLayers};

    BufferSpec bufferSpec = MinimumBufferSpec(kWidth, kRowsPerImage, kCopyLayers);
    bufferSpec.offset += 128u;
    bufferSpec.size += 128u;
    bufferSpec.rowsPerImage = kRowsPerImage;
    DoTest(textureSpec, bufferSpec, {kWidth, kHeight, kCopyLayers});
}

// Test a special code path in the D3D12 backends when (BytesPerRow * RowsPerImage) is a multiple
// of 512.
TEST_P(CopyTests_T2B, Texture2DArrayRegionWithOffsetEvenRowsPerImage) {
    constexpr uint32_t kWidth = 64;
    constexpr uint32_t kHeight = 128;
    constexpr uint32_t kLayers = 8u;
    constexpr uint32_t kBaseLayer = 2u;
    constexpr uint32_t kCopyLayers = 4u;

    constexpr uint32_t kRowsPerImage = kHeight + 2;

    TextureSpec textureSpec;
    textureSpec.copyOrigin = {0, 0, kBaseLayer};
    textureSpec.textureSize = {kWidth, kHeight, kLayers};

    BufferSpec bufferSpec = MinimumBufferSpec(kWidth, kRowsPerImage, kCopyLayers);
    bufferSpec.offset += 128u;
    bufferSpec.size += 128u;
    bufferSpec.rowsPerImage = kRowsPerImage;
    DoTest(textureSpec, bufferSpec, {kWidth, kHeight, kCopyLayers});
}

// Test that copying whole 3D texture in one texture-to-buffer-copy works.
TEST_P(CopyTests_T2B, Texture3DFull) {
    constexpr uint32_t kWidth = 256;
    constexpr uint32_t kHeight = 128;
    constexpr uint32_t kDepth = 6;

    TextureSpec textureSpec;
    textureSpec.textureSize = {kWidth, kHeight, kDepth};

    DoTest(textureSpec, MinimumBufferSpec(kWidth, kHeight, kDepth), {kWidth, kHeight, kDepth},
           wgpu::TextureDimension::e3D);
}

// Test that copying a range of texture 3D depths in one texture-to-buffer-copy works.
TEST_P(CopyTests_T2B, Texture3DSubRegion) {
    DAWN_TEST_UNSUPPORTED_IF(IsANGLE());  // TODO(crbug.com/angleproject/5967)

    constexpr uint32_t kWidth = 256;
    constexpr uint32_t kHeight = 128;
    constexpr uint32_t kDepth = 6;
    constexpr uint32_t kBaseDepth = 2u;
    constexpr uint32_t kCopyDepth = 3u;

    TextureSpec textureSpec;
    textureSpec.copyOrigin = {0, 0, kBaseDepth};
    textureSpec.textureSize = {kWidth, kHeight, kDepth};

    DoTest(textureSpec, MinimumBufferSpec(kWidth, kHeight, kCopyDepth),
           {kWidth / 2, kHeight / 2, kCopyDepth}, wgpu::TextureDimension::e3D);
}

TEST_P(CopyTests_T2B, Texture3DNoSplitRowDataWithEmptyFirstRow) {
    constexpr uint32_t kWidth = 2;
    constexpr uint32_t kHeight = 4;
    constexpr uint32_t kDepth = 3;

    TextureSpec textureSpec;
    textureSpec.textureSize = {kWidth, kHeight, kDepth};
    BufferSpec bufferSpec = MinimumBufferSpec(kWidth, kHeight, kDepth);

    // The tests below are designed to test TextureCopySplitter for 3D textures on D3D12.
    // Base: no split for a row + no empty first row
    bufferSpec.offset = 60;
    bufferSpec.size += bufferSpec.offset;
    DoTest(textureSpec, bufferSpec, {kWidth, kHeight, kDepth}, wgpu::TextureDimension::e3D);

    // This test will cover: no split for a row + empty first row
    bufferSpec.offset = 260;
    bufferSpec.size += bufferSpec.offset;
    DoTest(textureSpec, bufferSpec, {kWidth, kHeight, kDepth}, wgpu::TextureDimension::e3D);
}

TEST_P(CopyTests_T2B, Texture3DSplitRowDataWithoutEmptyFirstRow) {
    constexpr uint32_t kWidth = 259;
    constexpr uint32_t kHeight = 127;
    constexpr uint32_t kDepth = 3;

    TextureSpec textureSpec;
    textureSpec.textureSize = {kWidth, kHeight, kDepth};
    BufferSpec bufferSpec = MinimumBufferSpec(kWidth, kHeight, kDepth);

    // The test below is designed to test TextureCopySplitter for 3D textures on D3D12.
    // This test will cover: split for a row + no empty first row for both split regions
    bufferSpec.offset = 260;
    bufferSpec.size += bufferSpec.offset;
    DoTest(textureSpec, bufferSpec, {kWidth, kHeight, kDepth}, wgpu::TextureDimension::e3D);
}

TEST_P(CopyTests_T2B, Texture3DSplitRowDataWithEmptyFirstRow) {
    constexpr uint32_t kWidth = 39;
    constexpr uint32_t kHeight = 4;
    constexpr uint32_t kDepth = 3;

    TextureSpec textureSpec;
    textureSpec.textureSize = {kWidth, kHeight, kDepth};
    BufferSpec bufferSpec = MinimumBufferSpec(kWidth, kHeight, kDepth);

    // The tests below are designed to test TextureCopySplitter for 3D textures on D3D12.
    // This test will cover: split for a row + empty first row for the head block
    bufferSpec.offset = 400;
    bufferSpec.size += bufferSpec.offset;
    DoTest(textureSpec, bufferSpec, {kWidth, kHeight, kDepth}, wgpu::TextureDimension::e3D);

    // This test will cover: split for a row + empty first row for the tail block
    bufferSpec.offset = 160;
    bufferSpec.size += bufferSpec.offset;
    DoTest(textureSpec, bufferSpec, {kWidth, kHeight, kDepth}, wgpu::TextureDimension::e3D);
}

TEST_P(CopyTests_T2B, Texture3DCopyHeightIsOneCopyWidthIsTiny) {
    constexpr uint32_t kWidth = 2;
    constexpr uint32_t kHeight = 1;
    constexpr uint32_t kDepth = 3;

    TextureSpec textureSpec;
    textureSpec.textureSize = {kWidth, kHeight, kDepth};
    BufferSpec bufferSpec = MinimumBufferSpec(kWidth, kHeight, kDepth);

    // The tests below are designed to test TextureCopySplitter for 3D textures on D3D12.
    // Base: no split for a row, no empty row, and copy height is 1
    bufferSpec.offset = 60;
    bufferSpec.size += bufferSpec.offset;
    DoTest(textureSpec, bufferSpec, {kWidth, kHeight, kDepth}, wgpu::TextureDimension::e3D);

    // This test will cover: no split for a row + empty first row, and copy height is 1
    bufferSpec.offset = 260;
    bufferSpec.size += bufferSpec.offset;
    DoTest(textureSpec, bufferSpec, {kWidth, kHeight, kDepth}, wgpu::TextureDimension::e3D);
}

TEST_P(CopyTests_T2B, Texture3DCopyHeightIsOneCopyWidthIsSmall) {
    constexpr uint32_t kWidth = 39;
    constexpr uint32_t kHeight = 1;
    constexpr uint32_t kDepth = 3;

    TextureSpec textureSpec;
    textureSpec.textureSize = {kWidth, kHeight, kDepth};
    BufferSpec bufferSpec = MinimumBufferSpec(kWidth, kHeight, kDepth);

    // The tests below are designed to test TextureCopySplitter for 3D textures on D3D12.
    // This test will cover: split for a row + empty first row for the head block, and copy height
    // is 1
    bufferSpec.offset = 400;
    bufferSpec.size += bufferSpec.offset;
    DoTest(textureSpec, bufferSpec, {kWidth, kHeight, kDepth}, wgpu::TextureDimension::e3D);

    // This test will cover: split for a row + empty first row for the tail block, and copy height
    // is 1
    bufferSpec.offset = 160;
    bufferSpec.size += bufferSpec.offset;
    DoTest(textureSpec, bufferSpec, {kWidth, kHeight, kDepth}, wgpu::TextureDimension::e3D);
}

// Test that copying texture 3D array mips with 256-byte aligned sizes works
TEST_P(CopyTests_T2B, Texture3DMipAligned) {
    constexpr uint32_t kWidth = 256;
    constexpr uint32_t kHeight = 128;
    constexpr uint32_t kDepth = 64u;

    TextureSpec defaultTextureSpec;
    defaultTextureSpec.textureSize = {kWidth, kHeight, kDepth};

    for (unsigned int i = 1; i < 6; ++i) {
        TextureSpec textureSpec = defaultTextureSpec;
        textureSpec.copyLevel = i;
        textureSpec.levelCount = i + 1;

        DoTest(textureSpec, MinimumBufferSpec(kWidth >> i, kHeight >> i, kDepth >> i),
               {kWidth >> i, kHeight >> i, kDepth >> i}, wgpu::TextureDimension::e3D);
    }
}

// Test that copying texture 3D array mips with 256-byte unaligned sizes works
TEST_P(CopyTests_T2B, Texture3DMipUnaligned) {
    constexpr uint32_t kWidth = 261;
    constexpr uint32_t kHeight = 123;
    constexpr uint32_t kDepth = 69u;

    TextureSpec defaultTextureSpec;
    defaultTextureSpec.textureSize = {kWidth, kHeight, kDepth};

    for (unsigned int i = 1; i < 6; ++i) {
        TextureSpec textureSpec = defaultTextureSpec;
        textureSpec.copyLevel = i;
        textureSpec.levelCount = i + 1;

        DoTest(textureSpec, MinimumBufferSpec(kWidth >> i, kHeight >> i, kDepth >> i),
               {kWidth >> i, kHeight >> i, kDepth >> i}, wgpu::TextureDimension::e3D);
    }
}

DAWN_INSTANTIATE_TEST(CopyTests_T2B,
                      D3D12Backend(),
                      MetalBackend(),
                      OpenGLBackend(),
                      OpenGLESBackend(),
                      VulkanBackend());

// Test that copying an entire texture with 256-byte aligned dimensions works
TEST_P(CopyTests_B2T, FullTextureAligned) {
    constexpr uint32_t kWidth = 256;
    constexpr uint32_t kHeight = 128;

    TextureSpec textureSpec;
    textureSpec.textureSize = {kWidth, kHeight, 1};

    DoTest(textureSpec, MinimumBufferSpec(kWidth, kHeight), {kWidth, kHeight, 1});
}

// Test noop copies.
TEST_P(CopyTests_B2T, ZeroSizedCopy) {
    constexpr uint32_t kWidth = 256;
    constexpr uint32_t kHeight = 128;

    TextureSpec textureSpec;
    textureSpec.textureSize = {kWidth, kHeight, 1};

    DoTest(textureSpec, MinimumBufferSpec(kWidth, kHeight), {0, kHeight, 1});
    DoTest(textureSpec, MinimumBufferSpec(kWidth, kHeight), {kWidth, 0, 1});
    DoTest(textureSpec, MinimumBufferSpec(kWidth, kHeight), {kWidth, kHeight, 0});
}

// Test that copying an entire texture without 256-byte aligned dimensions works
TEST_P(CopyTests_B2T, FullTextureUnaligned) {
    constexpr uint32_t kWidth = 259;
    constexpr uint32_t kHeight = 127;

    TextureSpec textureSpec;
    textureSpec.textureSize = {kWidth, kHeight, 1};

    DoTest(textureSpec, MinimumBufferSpec(kWidth, kHeight), {kWidth, kHeight, 1});
}

// Test that reading pixels from a 256-byte aligned texture works
TEST_P(CopyTests_B2T, PixelReadAligned) {
    constexpr uint32_t kWidth = 256;
    constexpr uint32_t kHeight = 128;
    BufferSpec pixelBuffer = MinimumBufferSpec(1, 1);

    constexpr wgpu::Extent3D kCopySize = {1, 1, 1};
    constexpr wgpu::Extent3D kTextureSize = {kWidth, kHeight, 1};
    TextureSpec defaultTextureSpec;
    defaultTextureSpec.textureSize = kTextureSize;

    {
        TextureSpec textureSpec = defaultTextureSpec;
        DoTest(textureSpec, pixelBuffer, kCopySize);
    }

    {
        TextureSpec textureSpec = defaultTextureSpec;
        textureSpec.copyOrigin = {kWidth - 1, 0, 0};
        DoTest(textureSpec, pixelBuffer, kCopySize);
    }

    {
        TextureSpec textureSpec = defaultTextureSpec;
        textureSpec.copyOrigin = {0, kHeight - 1, 0};
        DoTest(textureSpec, pixelBuffer, kCopySize);
    }

    {
        TextureSpec textureSpec = defaultTextureSpec;
        textureSpec.copyOrigin = {kWidth - 1, kHeight - 1, 0};
        DoTest(textureSpec, pixelBuffer, kCopySize);
    }

    {
        TextureSpec textureSpec = defaultTextureSpec;
        textureSpec.copyOrigin = {kWidth / 3, kHeight / 7, 0};
        DoTest(textureSpec, pixelBuffer, kCopySize);
    }

    {
        TextureSpec textureSpec = defaultTextureSpec;
        textureSpec.copyOrigin = {kWidth / 7, kHeight / 3, 0};
        DoTest(textureSpec, pixelBuffer, kCopySize);
    }
}

// Test that copying pixels from a texture that is not 256-byte aligned works
TEST_P(CopyTests_B2T, PixelReadUnaligned) {
    constexpr uint32_t kWidth = 259;
    constexpr uint32_t kHeight = 127;
    BufferSpec pixelBuffer = MinimumBufferSpec(1, 1);

    constexpr wgpu::Extent3D kCopySize = {1, 1, 1};
    constexpr wgpu::Extent3D kTextureSize = {kWidth, kHeight, 1};
    TextureSpec defaultTextureSpec;
    defaultTextureSpec.textureSize = kTextureSize;

    {
        TextureSpec textureSpec = defaultTextureSpec;
        DoTest(textureSpec, pixelBuffer, kCopySize);
    }

    {
        TextureSpec textureSpec = defaultTextureSpec;
        textureSpec.copyOrigin = {kWidth - 1, 0, 0};
        DoTest(textureSpec, pixelBuffer, kCopySize);
    }

    {
        TextureSpec textureSpec = defaultTextureSpec;
        textureSpec.copyOrigin = {0, kHeight - 1, 0};
        DoTest(textureSpec, pixelBuffer, kCopySize);
    }

    {
        TextureSpec textureSpec = defaultTextureSpec;
        textureSpec.copyOrigin = {kWidth - 1, kHeight - 1, 0};
        DoTest(textureSpec, pixelBuffer, kCopySize);
    }

    {
        TextureSpec textureSpec = defaultTextureSpec;
        textureSpec.copyOrigin = {kWidth / 3, kHeight / 7, 0};
        DoTest(textureSpec, pixelBuffer, kCopySize);
    }

    {
        TextureSpec textureSpec = defaultTextureSpec;
        textureSpec.copyOrigin = {kWidth / 7, kHeight / 3, 0};
        DoTest(textureSpec, pixelBuffer, kCopySize);
    }
}

// Test that copying regions with 256-byte aligned sizes works
TEST_P(CopyTests_B2T, TextureRegionAligned) {
    constexpr uint32_t kWidth = 256;
    constexpr uint32_t kHeight = 128;
    for (unsigned int w : {64, 128, 256}) {
        for (unsigned int h : {16, 32, 48}) {
            TextureSpec textureSpec;
            textureSpec.textureSize = {kWidth, kHeight, 1};
            DoTest(textureSpec, MinimumBufferSpec(w, h), {w, h, 1});
        }
    }
}

// Test that copying regions without 256-byte aligned sizes works
TEST_P(CopyTests_B2T, TextureRegionUnaligned) {
    constexpr uint32_t kWidth = 256;
    constexpr uint32_t kHeight = 128;

    TextureSpec defaultTextureSpec;
    defaultTextureSpec.textureSize = {kWidth, kHeight, 1};

    for (unsigned int w : {13, 63, 65}) {
        for (unsigned int h : {17, 19, 63}) {
            TextureSpec textureSpec = defaultTextureSpec;
            DoTest(textureSpec, MinimumBufferSpec(w, h), {w, h, 1});
        }
    }
}

// Test that copying mips with 256-byte aligned sizes works
TEST_P(CopyTests_B2T, TextureMipAligned) {
    constexpr uint32_t kWidth = 256;
    constexpr uint32_t kHeight = 128;

    TextureSpec defaultTextureSpec;
    defaultTextureSpec.textureSize = {kWidth, kHeight, 1};

    for (unsigned int i = 1; i < 4; ++i) {
        TextureSpec textureSpec = defaultTextureSpec;
        textureSpec.copyLevel = i;
        textureSpec.levelCount = i + 1;
        DoTest(textureSpec, MinimumBufferSpec(kWidth >> i, kHeight >> i),
               {kWidth >> i, kHeight >> i, 1});
    }
}

// Test that copying mips without 256-byte aligned sizes works
TEST_P(CopyTests_B2T, TextureMipUnaligned) {
    constexpr uint32_t kWidth = 259;
    constexpr uint32_t kHeight = 127;

    TextureSpec defaultTextureSpec;
    defaultTextureSpec.textureSize = {kWidth, kHeight, 1};

    for (unsigned int i = 1; i < 4; ++i) {
        TextureSpec textureSpec = defaultTextureSpec;
        textureSpec.copyLevel = i;
        textureSpec.levelCount = i + 1;
        DoTest(textureSpec, MinimumBufferSpec(kWidth >> i, kHeight >> i),
               {kWidth >> i, kHeight >> i, 1});
    }
}

// Test that copying with a 512-byte aligned buffer offset works
TEST_P(CopyTests_B2T, OffsetBufferAligned) {
    constexpr uint32_t kWidth = 256;
    constexpr uint32_t kHeight = 128;

    TextureSpec textureSpec;
    textureSpec.textureSize = {kWidth, kHeight, 1};

    for (unsigned int i = 0; i < 3; ++i) {
        BufferSpec bufferSpec = MinimumBufferSpec(kWidth, kHeight);
        uint64_t offset = 512 * i;
        bufferSpec.size += offset;
        bufferSpec.offset += offset;
        DoTest(textureSpec, bufferSpec, {kWidth, kHeight, 1});
    }
}

// Test that copying without a 512-byte aligned buffer offset works
TEST_P(CopyTests_B2T, OffsetBufferUnaligned) {
    constexpr uint32_t kWidth = 256;
    constexpr uint32_t kHeight = 128;

    TextureSpec textureSpec;
    textureSpec.textureSize = {kWidth, kHeight, 1};

    const uint32_t bytesPerTexel = utils::GetTexelBlockSizeInBytes(textureSpec.format);
    for (uint32_t i = bytesPerTexel; i < 512; i += bytesPerTexel * 9) {
        BufferSpec bufferSpec = MinimumBufferSpec(kWidth, kHeight);
        bufferSpec.size += i;
        bufferSpec.offset += i;
        DoTest(textureSpec, bufferSpec, {kWidth, kHeight, 1});
    }
}

// Test that copying without a 512-byte aligned buffer offset that is greater than the bytes per row
// works
TEST_P(CopyTests_B2T, OffsetBufferUnalignedSmallBytesPerRow) {
    constexpr uint32_t kWidth = 32;
    constexpr uint32_t kHeight = 128;

    TextureSpec textureSpec;
    textureSpec.textureSize = {kWidth, kHeight, 1};

    const uint32_t bytesPerTexel = utils::GetTexelBlockSizeInBytes(textureSpec.format);
    for (uint32_t i = 256 + bytesPerTexel; i < 512; i += bytesPerTexel * 9) {
        BufferSpec bufferSpec = MinimumBufferSpec(kWidth, kHeight);
        bufferSpec.size += i;
        bufferSpec.offset += i;
        DoTest(textureSpec, bufferSpec, {kWidth, kHeight, 1});
    }
}

// Test that copying with a greater bytes per row than needed on a 256-byte aligned texture works
TEST_P(CopyTests_B2T, BytesPerRowAligned) {
    constexpr uint32_t kWidth = 256;
    constexpr uint32_t kHeight = 128;

    TextureSpec textureSpec;
    textureSpec.textureSize = {kWidth, kHeight, 1};

    BufferSpec bufferSpec = MinimumBufferSpec(kWidth, kHeight);
    for (unsigned int i = 1; i < 4; ++i) {
        bufferSpec.bytesPerRow += 256;
        bufferSpec.size += 256 * kHeight;
        DoTest(textureSpec, bufferSpec, {kWidth, kHeight, 1});
    }
}

// Test that copying with a greater bytes per row than needed on a texture that is not 256-byte
// aligned works
TEST_P(CopyTests_B2T, BytesPerRowUnaligned) {
    constexpr uint32_t kWidth = 259;
    constexpr uint32_t kHeight = 127;

    TextureSpec textureSpec;
    textureSpec.textureSize = {kWidth, kHeight, 1};

    BufferSpec bufferSpec = MinimumBufferSpec(kWidth, kHeight);
    for (unsigned int i = 1; i < 4; ++i) {
        bufferSpec.bytesPerRow += 256;
        bufferSpec.size += 256 * kHeight;
        DoTest(textureSpec, bufferSpec, {kWidth, kHeight, 1});
    }
}

// Test that copying with bytesPerRow = 0 and bytesPerRow < bytesInACompleteRow works
// when we're copying one row only
TEST_P(CopyTests_B2T, BytesPerRowWithOneRowCopy) {
    constexpr uint32_t kWidth = 259;
    constexpr uint32_t kHeight = 127;

    TextureSpec textureSpec;
    textureSpec.textureSize = {kWidth, kHeight, 1};

    {
        BufferSpec bufferSpec = MinimumBufferSpec(5, 1);

        // bytesPerRow undefined
        bufferSpec.bytesPerRow = wgpu::kCopyStrideUndefined;
        DoTest(textureSpec, bufferSpec, {5, 1, 1});
    }
}

TEST_P(CopyTests_B2T, StrideSpecialCases) {
    TextureSpec textureSpec;
    textureSpec.textureSize = {4, 4, 4};

    // bytesPerRow 0
    for (const wgpu::Extent3D copyExtent :
         {wgpu::Extent3D{0, 2, 2}, {0, 0, 2}, {0, 2, 0}, {0, 0, 0}}) {
        DoTest(textureSpec, MinimumBufferSpec(copyExtent, 0, 2), copyExtent);
    }

    // bytesPerRow undefined
    for (const wgpu::Extent3D copyExtent :
         {wgpu::Extent3D{2, 1, 1}, {2, 0, 1}, {2, 1, 0}, {2, 0, 0}}) {
        DoTest(textureSpec, MinimumBufferSpec(copyExtent, wgpu::kCopyStrideUndefined, 2),
               copyExtent);
    }

    // rowsPerImage 0
    for (const wgpu::Extent3D copyExtent :
         {wgpu::Extent3D{2, 0, 2}, {2, 0, 0}, {0, 0, 2}, {0, 0, 0}}) {
        DoTest(textureSpec, MinimumBufferSpec(copyExtent, 256, 0), copyExtent);
    }

    // rowsPerImage undefined
    for (const wgpu::Extent3D copyExtent : {wgpu::Extent3D{2, 2, 1}, {2, 2, 0}}) {
        DoTest(textureSpec, MinimumBufferSpec(copyExtent, 256, wgpu::kCopyStrideUndefined),
               copyExtent);
    }
}

// Test that copying whole texture 2D array layers in one texture-to-buffer-copy works.
TEST_P(CopyTests_B2T, Texture2DArrayFull) {
    constexpr uint32_t kWidth = 256;
    constexpr uint32_t kHeight = 128;
    constexpr uint32_t kLayers = 6u;

    TextureSpec textureSpec;
    textureSpec.textureSize = {kWidth, kHeight, kLayers};

    DoTest(textureSpec, MinimumBufferSpec(kWidth, kHeight, kLayers), {kWidth, kHeight, kLayers});
}

// Test that copying a range of texture 2D array layers in one texture-to-buffer-copy works.
TEST_P(CopyTests_B2T, Texture2DArraySubRegion) {
    constexpr uint32_t kWidth = 256;
    constexpr uint32_t kHeight = 128;
    constexpr uint32_t kLayers = 6u;
    constexpr uint32_t kBaseLayer = 2u;
    constexpr uint32_t kCopyLayers = 3u;

    TextureSpec textureSpec;
    textureSpec.copyOrigin = {0, 0, kBaseLayer};
    textureSpec.textureSize = {kWidth, kHeight, kLayers};

    DoTest(textureSpec, MinimumBufferSpec(kWidth, kHeight, kCopyLayers),
           {kWidth, kHeight, kCopyLayers});
}

// Test that copying into a range of texture 2D array layers in one texture-to-buffer-copy when
// RowsPerImage is not equal to the height of the texture works.
TEST_P(CopyTests_B2T, Texture2DArrayRegionNonzeroRowsPerImage) {
    constexpr uint32_t kWidth = 256;
    constexpr uint32_t kHeight = 128;
    constexpr uint32_t kLayers = 6u;
    constexpr uint32_t kBaseLayer = 2u;
    constexpr uint32_t kCopyLayers = 3u;

    constexpr uint32_t kRowsPerImage = kHeight * 2;

    TextureSpec textureSpec;
    textureSpec.copyOrigin = {0, 0, kBaseLayer};
    textureSpec.textureSize = {kWidth, kHeight, kLayers};

    BufferSpec bufferSpec = MinimumBufferSpec(kWidth, kRowsPerImage, kCopyLayers);
    bufferSpec.rowsPerImage = kRowsPerImage;
    DoTest(textureSpec, bufferSpec, {kWidth, kHeight, kCopyLayers});
}

// Test a special code path in the D3D12 backends when (BytesPerRow * RowsPerImage) is not a
// multiple of 512.
TEST_P(CopyTests_B2T, Texture2DArrayRegionWithOffsetOddRowsPerImage) {
    constexpr uint32_t kWidth = 64;
    constexpr uint32_t kHeight = 128;
    constexpr uint32_t kLayers = 8u;
    constexpr uint32_t kBaseLayer = 2u;
    constexpr uint32_t kCopyLayers = 5u;

    constexpr uint32_t kRowsPerImage = kHeight + 1;

    TextureSpec textureSpec;
    textureSpec.copyOrigin = {0, 0, kBaseLayer};
    textureSpec.textureSize = {kWidth, kHeight, kLayers};

    BufferSpec bufferSpec = MinimumBufferSpec(kWidth, kRowsPerImage, kCopyLayers);
    bufferSpec.offset += 128u;
    bufferSpec.size += 128u;
    bufferSpec.rowsPerImage = kRowsPerImage;
    DoTest(textureSpec, bufferSpec, {kWidth, kHeight, kCopyLayers});
}

// Test a special code path in the D3D12 backends when (BytesPerRow * RowsPerImage) is a multiple
// of 512.
TEST_P(CopyTests_B2T, Texture2DArrayRegionWithOffsetEvenRowsPerImage) {
    constexpr uint32_t kWidth = 64;
    constexpr uint32_t kHeight = 128;
    constexpr uint32_t kLayers = 8u;
    constexpr uint32_t kBaseLayer = 2u;
    constexpr uint32_t kCopyLayers = 5u;

    constexpr uint32_t kRowsPerImage = kHeight + 2;

    TextureSpec textureSpec;
    textureSpec.copyOrigin = {0, 0, kBaseLayer};
    textureSpec.textureSize = {kWidth, kHeight, kLayers};

    BufferSpec bufferSpec = MinimumBufferSpec(kWidth, kRowsPerImage, kCopyLayers);
    bufferSpec.offset += 128u;
    bufferSpec.size += 128u;
    bufferSpec.rowsPerImage = kRowsPerImage;
    DoTest(textureSpec, bufferSpec, {kWidth, kHeight, kCopyLayers});
}

// Test that copying whole texture 3D in one buffer-to-texture-copy works.
TEST_P(CopyTests_B2T, Texture3DFull) {
    constexpr uint32_t kWidth = 256;
    constexpr uint32_t kHeight = 128;
    constexpr uint32_t kDepth = 6;

    TextureSpec textureSpec;
    textureSpec.textureSize = {kWidth, kHeight, kDepth};

    DoTest(textureSpec, MinimumBufferSpec(kWidth, kHeight, kDepth), {kWidth, kHeight, kDepth},
           wgpu::TextureDimension::e3D);
}

// Test that copying a range of texture 3D Depths in one texture-to-buffer-copy works.
TEST_P(CopyTests_B2T, Texture3DSubRegion) {
    DAWN_TEST_UNSUPPORTED_IF(IsANGLE());  // TODO(crbug.com/angleproject/5967)

    constexpr uint32_t kWidth = 256;
    constexpr uint32_t kHeight = 128;
    constexpr uint32_t kDepth = 6;
    constexpr uint32_t kBaseDepth = 2u;
    constexpr uint32_t kCopyDepth = 3u;

    TextureSpec textureSpec;
    textureSpec.copyOrigin = {0, 0, kBaseDepth};
    textureSpec.textureSize = {kWidth, kHeight, kDepth};

    DoTest(textureSpec, MinimumBufferSpec(kWidth, kHeight, kCopyDepth),
           {kWidth / 2, kHeight / 2, kCopyDepth}, wgpu::TextureDimension::e3D);
}

TEST_P(CopyTests_B2T, Texture3DNoSplitRowDataWithEmptyFirstRow) {
    constexpr uint32_t kWidth = 2;
    constexpr uint32_t kHeight = 4;
    constexpr uint32_t kDepth = 3;

    TextureSpec textureSpec;
    textureSpec.textureSize = {kWidth, kHeight, kDepth};
    BufferSpec bufferSpec = MinimumBufferSpec(kWidth, kHeight, kDepth);

    // The tests below are designed to test TextureCopySplitter for 3D textures on D3D12.
    // Base: no split for a row + no empty first row
    bufferSpec.offset = 60;
    bufferSpec.size += bufferSpec.offset;
    DoTest(textureSpec, bufferSpec, {kWidth, kHeight, kDepth}, wgpu::TextureDimension::e3D);

    // This test will cover: no split for a row + empty first row
    bufferSpec.offset = 260;
    bufferSpec.size += bufferSpec.offset;
    DoTest(textureSpec, bufferSpec, {kWidth, kHeight, kDepth}, wgpu::TextureDimension::e3D);
}

TEST_P(CopyTests_B2T, Texture3DSplitRowDataWithoutEmptyFirstRow) {
    constexpr uint32_t kWidth = 259;
    constexpr uint32_t kHeight = 127;
    constexpr uint32_t kDepth = 3;

    TextureSpec textureSpec;
    textureSpec.textureSize = {kWidth, kHeight, kDepth};
    BufferSpec bufferSpec = MinimumBufferSpec(kWidth, kHeight, kDepth);

    // The test below is designed to test TextureCopySplitter for 3D textures on D3D12.
    // This test will cover: split for a row + no empty first row for both split regions
    bufferSpec.offset = 260;
    bufferSpec.size += bufferSpec.offset;
    DoTest(textureSpec, bufferSpec, {kWidth, kHeight, kDepth}, wgpu::TextureDimension::e3D);
}

TEST_P(CopyTests_B2T, Texture3DSplitRowDataWithEmptyFirstRow) {
    constexpr uint32_t kWidth = 39;
    constexpr uint32_t kHeight = 4;
    constexpr uint32_t kDepth = 3;

    TextureSpec textureSpec;
    textureSpec.textureSize = {kWidth, kHeight, kDepth};
    BufferSpec bufferSpec = MinimumBufferSpec(kWidth, kHeight, kDepth);

    // The tests below are designed to test TextureCopySplitter for 3D textures on D3D12.
    // This test will cover: split for a row + empty first row for the head block
    bufferSpec.offset = 400;
    bufferSpec.size += bufferSpec.offset;
    DoTest(textureSpec, bufferSpec, {kWidth, kHeight, kDepth}, wgpu::TextureDimension::e3D);

    // This test will cover: split for a row + empty first row for the tail block
    bufferSpec.offset = 160;
    bufferSpec.size += bufferSpec.offset;
    DoTest(textureSpec, bufferSpec, {kWidth, kHeight, kDepth}, wgpu::TextureDimension::e3D);
}

TEST_P(CopyTests_B2T, Texture3DCopyHeightIsOneCopyWidthIsTiny) {
    constexpr uint32_t kWidth = 2;
    constexpr uint32_t kHeight = 1;
    constexpr uint32_t kDepth = 3;

    TextureSpec textureSpec;
    textureSpec.textureSize = {kWidth, kHeight, kDepth};
    BufferSpec bufferSpec = MinimumBufferSpec(kWidth, kHeight, kDepth);

    // The tests below are designed to test TextureCopySplitter for 3D textures on D3D12.
    // Base: no split for a row, no empty row, and copy height is 1
    bufferSpec.offset = 60;
    bufferSpec.size += bufferSpec.offset;
    DoTest(textureSpec, bufferSpec, {kWidth, kHeight, kDepth}, wgpu::TextureDimension::e3D);

    // This test will cover: no split for a row + empty first row, and copy height is 1
    bufferSpec.offset = 260;
    bufferSpec.size += bufferSpec.offset;
    DoTest(textureSpec, bufferSpec, {kWidth, kHeight, kDepth}, wgpu::TextureDimension::e3D);
}

TEST_P(CopyTests_B2T, Texture3DCopyHeightIsOneCopyWidthIsSmall) {
    constexpr uint32_t kWidth = 39;
    constexpr uint32_t kHeight = 1;
    constexpr uint32_t kDepth = 3;

    TextureSpec textureSpec;
    textureSpec.textureSize = {kWidth, kHeight, kDepth};
    BufferSpec bufferSpec = MinimumBufferSpec(kWidth, kHeight, kDepth);

    // The tests below are designed to test TextureCopySplitter for 3D textures on D3D12.
    // This test will cover: split for a row + empty first row for the head block, and copy height
    // is 1
    bufferSpec.offset = 400;
    bufferSpec.size += bufferSpec.offset;
    DoTest(textureSpec, bufferSpec, {kWidth, kHeight, kDepth}, wgpu::TextureDimension::e3D);

    // This test will cover: split for a row + empty first row for the tail block, and copy height
    // is 1
    bufferSpec.offset = 160;
    bufferSpec.size += bufferSpec.offset;
    DoTest(textureSpec, bufferSpec, {kWidth, kHeight, kDepth}, wgpu::TextureDimension::e3D);
}

// Test that copying texture 3D array mips with 256-byte aligned sizes works
TEST_P(CopyTests_B2T, Texture3DMipAligned) {
    constexpr uint32_t kWidth = 256;
    constexpr uint32_t kHeight = 128;
    constexpr uint32_t kDepth = 64u;

    TextureSpec defaultTextureSpec;
    defaultTextureSpec.textureSize = {kWidth, kHeight, kDepth};

    for (unsigned int i = 1; i < 6; ++i) {
        TextureSpec textureSpec = defaultTextureSpec;
        textureSpec.copyLevel = i;
        textureSpec.levelCount = i + 1;

        DoTest(textureSpec, MinimumBufferSpec(kWidth >> i, kHeight >> i, kDepth >> i),
               {kWidth >> i, kHeight >> i, kDepth >> i}, wgpu::TextureDimension::e3D);
    }
}

// Test that copying texture 3D array mips with 256-byte unaligned sizes works
TEST_P(CopyTests_B2T, Texture3DMipUnaligned) {
    constexpr uint32_t kWidth = 261;
    constexpr uint32_t kHeight = 123;
    constexpr uint32_t kDepth = 69u;

    TextureSpec defaultTextureSpec;
    defaultTextureSpec.textureSize = {kWidth, kHeight, kDepth};

    for (unsigned int i = 1; i < 6; ++i) {
        TextureSpec textureSpec = defaultTextureSpec;
        textureSpec.copyLevel = i;
        textureSpec.levelCount = i + 1;

        DoTest(textureSpec, MinimumBufferSpec(kWidth >> i, kHeight >> i, kDepth >> i),
               {kWidth >> i, kHeight >> i, kDepth >> i}, wgpu::TextureDimension::e3D);
    }
}

DAWN_INSTANTIATE_TEST(CopyTests_B2T,
                      D3D12Backend(),
                      MetalBackend(),
                      OpenGLBackend(),
                      OpenGLESBackend(),
                      VulkanBackend());

TEST_P(CopyTests_T2T, Texture) {
    constexpr uint32_t kWidth = 256;
    constexpr uint32_t kHeight = 128;

    TextureSpec textureSpec;
    textureSpec.textureSize = {kWidth, kHeight, 1};
    DoTest(textureSpec, textureSpec, {kWidth, kHeight, 1});
}

// Test noop copies.
TEST_P(CopyTests_T2T, ZeroSizedCopy) {
    constexpr uint32_t kWidth = 256;
    constexpr uint32_t kHeight = 128;

    TextureSpec textureSpec;
    textureSpec.textureSize = {kWidth, kHeight, 1};
    DoTest(textureSpec, textureSpec, {0, kHeight, 1});
    DoTest(textureSpec, textureSpec, {kWidth, 0, 1});
    DoTest(textureSpec, textureSpec, {kWidth, kHeight, 0});
}

TEST_P(CopyTests_T2T, TextureRegion) {
    constexpr uint32_t kWidth = 256;
    constexpr uint32_t kHeight = 128;

    TextureSpec defaultTextureSpec;
    defaultTextureSpec.textureSize = {kWidth, kHeight, 1};

    for (unsigned int w : {64, 128, 256}) {
        for (unsigned int h : {16, 32, 48}) {
            TextureSpec textureSpec = defaultTextureSpec;
            DoTest(textureSpec, textureSpec, {w, h, 1});
        }
    }
}

TEST_P(CopyTests_T2T, TextureMip) {
    constexpr uint32_t kWidth = 256;
    constexpr uint32_t kHeight = 128;

    TextureSpec defaultTextureSpec;
    defaultTextureSpec.textureSize = {kWidth, kHeight, 1};

    for (unsigned int i = 1; i < 4; ++i) {
        TextureSpec textureSpec = defaultTextureSpec;
        textureSpec.copyLevel = i;
        textureSpec.levelCount = i + 1;

        DoTest(textureSpec, textureSpec, {kWidth >> i, kHeight >> i, 1});
    }
}

TEST_P(CopyTests_T2T, SingleMipSrcMultipleMipDst) {
    constexpr uint32_t kWidth = 256;
    constexpr uint32_t kHeight = 128;

    TextureSpec defaultTextureSpec;

    for (unsigned int i = 1; i < 4; ++i) {
        TextureSpec srcTextureSpec = defaultTextureSpec;
        srcTextureSpec.textureSize = {kWidth >> i, kHeight >> i, 1};

        TextureSpec dstTextureSpec = defaultTextureSpec;
        dstTextureSpec.textureSize = {kWidth, kHeight, 1};
        dstTextureSpec.copyLevel = i;
        dstTextureSpec.levelCount = i + 1;

        DoTest(srcTextureSpec, dstTextureSpec, {kWidth >> i, kHeight >> i, 1});
    }
}

TEST_P(CopyTests_T2T, MultipleMipSrcSingleMipDst) {
    constexpr uint32_t kWidth = 256;
    constexpr uint32_t kHeight = 128;

    TextureSpec defaultTextureSpec;

    for (unsigned int i = 1; i < 4; ++i) {
        TextureSpec srcTextureSpec = defaultTextureSpec;
        srcTextureSpec.textureSize = {kWidth, kHeight, 1};
        srcTextureSpec.copyLevel = i;
        srcTextureSpec.levelCount = i + 1;

        TextureSpec dstTextureSpec = defaultTextureSpec;
        dstTextureSpec.textureSize = {kWidth >> i, kHeight >> i, 1};

        DoTest(srcTextureSpec, dstTextureSpec, {kWidth >> i, kHeight >> i, 1});
    }
}

// Test that copying from one mip level to another mip level within the same 2D texture works.
TEST_P(CopyTests_T2T, Texture2DSameTextureDifferentMipLevels) {
    constexpr uint32_t kWidth = 256;
    constexpr uint32_t kHeight = 128;

    TextureSpec defaultTextureSpec;
    defaultTextureSpec.textureSize = {kWidth, kHeight, 1};
    defaultTextureSpec.levelCount = 6;

    for (unsigned int i = 1; i < 6; ++i) {
        TextureSpec srcSpec = defaultTextureSpec;
        srcSpec.copyLevel = i - 1;
        TextureSpec dstSpec = defaultTextureSpec;
        dstSpec.copyLevel = i;

        DoTest(srcSpec, dstSpec, {kWidth >> i, kHeight >> i, 1}, true);
    }
}

// Test copying the whole 2D array texture.
TEST_P(CopyTests_T2T, Texture2DArrayFull) {
    constexpr uint32_t kWidth = 256;
    constexpr uint32_t kHeight = 128;
    constexpr uint32_t kLayers = 6u;

    TextureSpec textureSpec;
    textureSpec.textureSize = {kWidth, kHeight, kLayers};

    DoTest(textureSpec, textureSpec, {kWidth, kHeight, kLayers});
}

// Test copying a subresource region of the 2D array texture.
TEST_P(CopyTests_T2T, Texture2DArrayRegion) {
    constexpr uint32_t kWidth = 256;
    constexpr uint32_t kHeight = 128;
    constexpr uint32_t kLayers = 6u;

    TextureSpec defaultTextureSpec;
    defaultTextureSpec.textureSize = {kWidth, kHeight, kLayers};

    for (unsigned int w : {64, 128, 256}) {
        for (unsigned int h : {16, 32, 48}) {
            TextureSpec textureSpec = defaultTextureSpec;
            DoTest(textureSpec, textureSpec, {w, h, kLayers});
        }
    }
}

// Test copying one slice of a 2D array texture.
TEST_P(CopyTests_T2T, Texture2DArrayCopyOneSlice) {
    constexpr uint32_t kWidth = 256;
    constexpr uint32_t kHeight = 128;
    constexpr uint32_t kLayers = 6u;
    constexpr uint32_t kSrcBaseLayer = 1u;
    constexpr uint32_t kDstBaseLayer = 3u;
    constexpr uint32_t kCopyArrayLayerCount = 1u;

    TextureSpec defaultTextureSpec;
    defaultTextureSpec.textureSize = {kWidth, kHeight, kLayers};

    TextureSpec srcTextureSpec = defaultTextureSpec;
    srcTextureSpec.copyOrigin = {0, 0, kSrcBaseLayer};

    TextureSpec dstTextureSpec = defaultTextureSpec;
    dstTextureSpec.copyOrigin = {0, 0, kDstBaseLayer};

    DoTest(srcTextureSpec, dstTextureSpec, {kWidth, kHeight, kCopyArrayLayerCount});
}

// Test copying multiple contiguous slices of a 2D array texture.
TEST_P(CopyTests_T2T, Texture2DArrayCopyMultipleSlices) {
    constexpr uint32_t kWidth = 256;
    constexpr uint32_t kHeight = 128;
    constexpr uint32_t kLayers = 6u;
    constexpr uint32_t kSrcBaseLayer = 0u;
    constexpr uint32_t kDstBaseLayer = 3u;
    constexpr uint32_t kCopyArrayLayerCount = 3u;

    TextureSpec defaultTextureSpec;
    defaultTextureSpec.textureSize = {kWidth, kHeight, kLayers};

    TextureSpec srcTextureSpec = defaultTextureSpec;
    srcTextureSpec.copyOrigin = {0, 0, kSrcBaseLayer};

    TextureSpec dstTextureSpec = defaultTextureSpec;
    dstTextureSpec.copyOrigin = {0, 0, kDstBaseLayer};

    DoTest(srcTextureSpec, dstTextureSpec, {kWidth, kHeight, kCopyArrayLayerCount});
}

// Test copying one texture slice within the same texture.
TEST_P(CopyTests_T2T, CopyWithinSameTextureOneSlice) {
    constexpr uint32_t kWidth = 256u;
    constexpr uint32_t kHeight = 128u;
    constexpr uint32_t kLayers = 6u;
    constexpr uint32_t kSrcBaseLayer = 0u;
    constexpr uint32_t kDstBaseLayer = 3u;
    constexpr uint32_t kCopyArrayLayerCount = 1u;

    TextureSpec defaultTextureSpec;
    defaultTextureSpec.textureSize = {kWidth, kHeight, kLayers};

    TextureSpec srcTextureSpec = defaultTextureSpec;
    srcTextureSpec.copyOrigin = {0, 0, kSrcBaseLayer};

    TextureSpec dstTextureSpec = defaultTextureSpec;
    dstTextureSpec.copyOrigin = {0, 0, kDstBaseLayer};

    DoTest(srcTextureSpec, dstTextureSpec, {kWidth, kHeight, kCopyArrayLayerCount}, true);
}

// Test copying multiple contiguous texture slices within the same texture with non-overlapped
// slices.
TEST_P(CopyTests_T2T, CopyWithinSameTextureNonOverlappedSlices) {
    constexpr uint32_t kWidth = 256u;
    constexpr uint32_t kHeight = 128u;
    constexpr uint32_t kLayers = 6u;
    constexpr uint32_t kSrcBaseLayer = 0u;
    constexpr uint32_t kDstBaseLayer = 3u;
    constexpr uint32_t kCopyArrayLayerCount = 3u;

    TextureSpec defaultTextureSpec;
    defaultTextureSpec.textureSize = {kWidth, kHeight, kLayers};

    TextureSpec srcTextureSpec = defaultTextureSpec;
    srcTextureSpec.copyOrigin = {0, 0, kSrcBaseLayer};

    TextureSpec dstTextureSpec = defaultTextureSpec;
    dstTextureSpec.copyOrigin = {0, 0, kDstBaseLayer};

    DoTest(srcTextureSpec, dstTextureSpec, {kWidth, kHeight, kCopyArrayLayerCount}, true);
}

// A regression test (from WebGPU CTS) for an Intel D3D12 driver bug about T2T copy with specific
// texture formats. See http://crbug.com/1161355 for more details.
TEST_P(CopyTests_T2T, CopyFromNonZeroMipLevelWithTexelBlockSizeLessThan4Bytes) {
    // This test can pass on the Windows Intel Vulkan driver version 27.20.100.9168.
    // TODO(crbug.com/dawn/819): enable this test on Intel Vulkan drivers after the upgrade of
    // try bots.
    DAWN_SUPPRESS_TEST_IF(IsVulkan() && IsWindows() && IsIntel());

    constexpr std::array<wgpu::TextureFormat, 11> kFormats = {
        {wgpu::TextureFormat::RG8Sint, wgpu::TextureFormat::RG8Uint, wgpu::TextureFormat::RG8Snorm,
         wgpu::TextureFormat::RG8Unorm, wgpu::TextureFormat::R16Float, wgpu::TextureFormat::R16Sint,
         wgpu::TextureFormat::R16Uint, wgpu::TextureFormat::R8Snorm, wgpu::TextureFormat::R8Unorm,
         wgpu::TextureFormat::R8Sint, wgpu::TextureFormat::R8Uint}};

    constexpr uint32_t kSrcLevelCount = 4;
    constexpr uint32_t kDstLevelCount = 5;
    constexpr uint32_t kSrcSize = 2 << kSrcLevelCount;
    constexpr uint32_t kDstSize = 2 << kDstLevelCount;
    ASSERT_LE(kSrcSize, kTextureBytesPerRowAlignment);
    ASSERT_LE(kDstSize, kTextureBytesPerRowAlignment);

    // The copyLayer to test:
    // 1u (non-array texture), 3u (copyLayer < copyWidth), 5u (copyLayer > copyWidth)
    constexpr std::array<uint32_t, 3> kTestTextureLayer = {1u, 3u, 5u};

    for (wgpu::TextureFormat format : kFormats) {
        if (HasToggleEnabled("disable_snorm_read") &&
            (format == wgpu::TextureFormat::RG8Snorm || format == wgpu::TextureFormat::R8Snorm)) {
            continue;
        }

        if (HasToggleEnabled("disable_r8_rg8_mipmaps") &&
            (format == wgpu::TextureFormat::R8Unorm || format == wgpu::TextureFormat::RG8Unorm)) {
            continue;
        }

        for (uint32_t textureLayer : kTestTextureLayer) {
            const wgpu::Extent3D kUploadSize = {4u, 4u, textureLayer};

            for (uint32_t srcLevel = 0; srcLevel < kSrcLevelCount; ++srcLevel) {
                for (uint32_t dstLevel = 0; dstLevel < kDstLevelCount; ++dstLevel) {
                    TextureSpec srcSpec;
                    srcSpec.levelCount = kSrcLevelCount;
                    srcSpec.format = format;
                    srcSpec.copyLevel = srcLevel;
                    srcSpec.textureSize = {kSrcSize, kSrcSize, textureLayer};

                    TextureSpec dstSpec = srcSpec;
                    dstSpec.levelCount = kDstLevelCount;
                    dstSpec.copyLevel = dstLevel;
                    dstSpec.textureSize = {kDstSize, kDstSize, textureLayer};

                    DoTest(srcSpec, dstSpec, kUploadSize);
                }
            }
        }
    }
}

// Test that copying from one mip level to another mip level within the same 2D array texture works.
TEST_P(CopyTests_T2T, Texture2DArraySameTextureDifferentMipLevels) {
    constexpr uint32_t kWidth = 256;
    constexpr uint32_t kHeight = 128;
    constexpr uint32_t kLayers = 8u;

    TextureSpec defaultTextureSpec;
    defaultTextureSpec.textureSize = {kWidth, kHeight, kLayers};
    defaultTextureSpec.levelCount = 6;

    for (unsigned int i = 1; i < 6; ++i) {
        TextureSpec srcSpec = defaultTextureSpec;
        srcSpec.copyLevel = i - 1;
        TextureSpec dstSpec = defaultTextureSpec;
        dstSpec.copyLevel = i;

        DoTest(srcSpec, dstSpec, {kWidth >> i, kHeight >> i, kLayers}, true);
    }
}

// Test that copying whole 3D texture in one texture-to-texture-copy works.
TEST_P(CopyTests_T2T, Texture3DFull) {
    constexpr uint32_t kWidth = 256;
    constexpr uint32_t kHeight = 128;
    constexpr uint32_t kDepth = 6u;

    TextureSpec textureSpec;
    textureSpec.textureSize = {kWidth, kHeight, kDepth};

    DoTest(textureSpec, textureSpec, {kWidth, kHeight, kDepth}, false, wgpu::TextureDimension::e3D);
}

// Test that copying from one mip level to another mip level within the same 3D texture works.
TEST_P(CopyTests_T2T, Texture3DSameTextureDifferentMipLevels) {
    constexpr uint32_t kWidth = 256;
    constexpr uint32_t kHeight = 128;
    constexpr uint32_t kDepth = 6u;

    TextureSpec textureSpec;
    textureSpec.textureSize = {kWidth, kHeight, kDepth};
    textureSpec.levelCount = 2;

    TextureSpec dstSpec = textureSpec;
    dstSpec.copyLevel = 1;

    DoTest(textureSpec, dstSpec, {kWidth >> 1, kHeight >> 1, kDepth >> 1}, true,
           wgpu::TextureDimension::e3D);
}

// Test that copying whole 3D texture to a 2D array in one texture-to-texture-copy works.
TEST_P(CopyTests_T2T, Texture3DTo2DArrayFull) {
    constexpr uint32_t kWidth = 256;
    constexpr uint32_t kHeight = 128;
    constexpr uint32_t kDepth = 6u;

    TextureSpec textureSpec;
    textureSpec.textureSize = {kWidth, kHeight, kDepth};

    DoTest(textureSpec, textureSpec, {kWidth, kHeight, kDepth}, wgpu::TextureDimension::e3D,
           wgpu::TextureDimension::e2D);
}

// Test that copying between 3D texture and 2D array textures works. It includes partial copy
// for src and/or dst texture, non-zero offset (copy origin), non-zero mip level.
TEST_P(CopyTests_T2T, Texture3DAnd2DArraySubRegion) {
    DAWN_TEST_UNSUPPORTED_IF(IsANGLE());  // TODO(crbug.com/angleproject/5967)
    // TODO(crbug.com/dawn/1216): Remove this suppression.
    DAWN_SUPPRESS_TEST_IF(IsD3D12() && IsNvidia());

    constexpr uint32_t kWidth = 8;
    constexpr uint32_t kHeight = 4;
    constexpr uint32_t kDepth = 2u;

    TextureSpec baseSpec;
    baseSpec.textureSize = {kWidth, kHeight, kDepth};

    TextureSpec srcSpec = baseSpec;
    TextureSpec dstSpec = baseSpec;

    // dst texture is a partial copy
    dstSpec.textureSize = {kWidth * 2, kHeight * 2, kDepth * 2};
    DoTest(srcSpec, dstSpec, {kWidth, kHeight, kDepth}, wgpu::TextureDimension::e3D,
           wgpu::TextureDimension::e2D);
    DoTest(srcSpec, dstSpec, {kWidth, kHeight, kDepth}, wgpu::TextureDimension::e2D,
           wgpu::TextureDimension::e3D);

    // src texture is a partial copy
    srcSpec.textureSize = {kWidth * 2, kHeight * 2, kDepth * 2};
    dstSpec = baseSpec;
    DoTest(srcSpec, dstSpec, {kWidth, kHeight, kDepth}, wgpu::TextureDimension::e3D,
           wgpu::TextureDimension::e2D);
    DoTest(srcSpec, dstSpec, {kWidth, kHeight, kDepth}, wgpu::TextureDimension::e2D,
           wgpu::TextureDimension::e3D);

    // Both src and dst texture is a partial copy
    srcSpec.textureSize = {kWidth * 2, kHeight * 2, kDepth * 2};
    dstSpec.textureSize = {kWidth * 2, kHeight * 2, kDepth * 2};
    DoTest(srcSpec, dstSpec, {kWidth, kHeight, kDepth}, wgpu::TextureDimension::e3D,
           wgpu::TextureDimension::e2D);
    DoTest(srcSpec, dstSpec, {kWidth, kHeight, kDepth}, wgpu::TextureDimension::e2D,
           wgpu::TextureDimension::e3D);

    // Non-zero offset (copy origin)
    srcSpec = baseSpec;
    dstSpec.textureSize = {kWidth * 2, kHeight * 2, kDepth * 2};
    dstSpec.copyOrigin = {kWidth, kHeight, kDepth};
    DoTest(srcSpec, dstSpec, {kWidth, kHeight, kDepth}, wgpu::TextureDimension::e3D,
           wgpu::TextureDimension::e2D);
    DoTest(srcSpec, dstSpec, {kWidth, kHeight, kDepth}, wgpu::TextureDimension::e2D,
           wgpu::TextureDimension::e3D);

    // Non-zero mip level
    srcSpec = baseSpec;
    dstSpec.textureSize = {kWidth * 2, kHeight * 2, kDepth * 2};
    dstSpec.copyOrigin = {0, 0, 0};
    dstSpec.copyLevel = 1;
    dstSpec.levelCount = 2;
    DoTest(srcSpec, dstSpec, {kWidth, kHeight, kDepth}, wgpu::TextureDimension::e3D,
           wgpu::TextureDimension::e2D);
    DoTest(srcSpec, dstSpec, {kWidth, kHeight, kDepth}, wgpu::TextureDimension::e2D,
           wgpu::TextureDimension::e3D);
}

// Test that copying whole 2D array to a 3D texture in one texture-to-texture-copy works.
TEST_P(CopyTests_T2T, Texture2DArrayTo3DFull) {
    // TODO(crbug.com/dawn/1216): Remove this suppression.
    DAWN_SUPPRESS_TEST_IF(IsD3D12() && IsNvidia());
    constexpr uint32_t kWidth = 256;
    constexpr uint32_t kHeight = 128;
    constexpr uint32_t kDepth = 6u;

    TextureSpec textureSpec;
    textureSpec.textureSize = {kWidth, kHeight, kDepth};

    DoTest(textureSpec, textureSpec, {kWidth, kHeight, kDepth}, wgpu::TextureDimension::e2D,
           wgpu::TextureDimension::e3D);
}

// Test that copying subregion of a 3D texture in one texture-to-texture-copy works.
TEST_P(CopyTests_T2T, Texture3DSubRegion) {
    DAWN_TEST_UNSUPPORTED_IF(IsANGLE());  // TODO(crbug.com/angleproject/5967)
    constexpr uint32_t kWidth = 256;
    constexpr uint32_t kHeight = 128;
    constexpr uint32_t kDepth = 6u;

    TextureSpec textureSpec;
    textureSpec.textureSize = {kWidth, kHeight, kDepth};

    DoTest(textureSpec, textureSpec, {kWidth / 2, kHeight / 2, kDepth / 2}, false,
           wgpu::TextureDimension::e3D);
}

// Test that copying subregion of a 3D texture to a 2D array in one texture-to-texture-copy works.
TEST_P(CopyTests_T2T, Texture3DTo2DArraySubRegion) {
    constexpr uint32_t kWidth = 256;
    constexpr uint32_t kHeight = 128;
    constexpr uint32_t kDepth = 6u;

    TextureSpec textureSpec;
    textureSpec.textureSize = {kWidth, kHeight, kDepth};

    DoTest(textureSpec, textureSpec, {kWidth / 2, kHeight / 2, kDepth / 2},
           wgpu::TextureDimension::e3D, wgpu::TextureDimension::e2D);
}

// Test that copying subregion of a 2D array to a 3D texture to in one texture-to-texture-copy
// works.
TEST_P(CopyTests_T2T, Texture2DArrayTo3DSubRegion) {
    DAWN_TEST_UNSUPPORTED_IF(IsANGLE());  // TODO(crbug.com/angleproject/5967)
    // TODO(crbug.com/dawn/1216): Remove this suppression.
    DAWN_SUPPRESS_TEST_IF(IsD3D12() && IsNvidia());
    constexpr uint32_t kWidth = 256;
    constexpr uint32_t kHeight = 128;
    constexpr uint32_t kDepth = 6u;

    TextureSpec textureSpec;
    textureSpec.textureSize = {kWidth, kHeight, kDepth};

    DoTest(textureSpec, textureSpec, {kWidth / 2, kHeight / 2, kDepth / 2},
           wgpu::TextureDimension::e2D, wgpu::TextureDimension::e3D);
}

// Test that copying texture 3D array mips in one texture-to-texture-copy works
TEST_P(CopyTests_T2T, Texture3DMipAligned) {
    constexpr uint32_t kWidth = 256;
    constexpr uint32_t kHeight = 128;
    constexpr uint32_t kDepth = 64u;

    TextureSpec defaultTextureSpec;
    defaultTextureSpec.textureSize = {kWidth, kHeight, kDepth};

    for (unsigned int i = 1; i < 6; ++i) {
        TextureSpec textureSpec = defaultTextureSpec;
        textureSpec.copyLevel = i;
        textureSpec.levelCount = i + 1;

        DoTest(textureSpec, textureSpec, {kWidth >> i, kHeight >> i, kDepth >> i},
               wgpu::TextureDimension::e3D, wgpu::TextureDimension::e3D);
    }
}

// Test that copying texture 3D array mips in one texture-to-texture-copy works
TEST_P(CopyTests_T2T, Texture3DMipUnaligned) {
    constexpr uint32_t kWidth = 261;
    constexpr uint32_t kHeight = 123;
    constexpr uint32_t kDepth = 69u;

    TextureSpec defaultTextureSpec;
    defaultTextureSpec.textureSize = {kWidth, kHeight, kDepth};

    for (unsigned int i = 1; i < 6; ++i) {
        TextureSpec textureSpec = defaultTextureSpec;
        textureSpec.copyLevel = i;
        textureSpec.levelCount = i + 1;

        DoTest(textureSpec, textureSpec, {kWidth >> i, kHeight >> i, kDepth >> i},
               wgpu::TextureDimension::e3D, wgpu::TextureDimension::e3D);
    }
}

DAWN_INSTANTIATE_TEST_P(CopyTests_T2T,
                        {D3D12Backend(),
                         D3D12Backend({"use_temp_buffer_in_small_format_texture_to_texture_copy_"
                                       "from_greater_to_less_mip_level"}),
                         MetalBackend(), OpenGLBackend(), OpenGLESBackend(), VulkanBackend()},
                        {true, false});

static constexpr uint64_t kSmallBufferSize = 4;
static constexpr uint64_t kLargeBufferSize = 1 << 16;

// Test copying full buffers
TEST_P(CopyTests_B2B, FullCopy) {
    DoTest(kSmallBufferSize, 0, kSmallBufferSize, 0, kSmallBufferSize);
    DoTest(kLargeBufferSize, 0, kLargeBufferSize, 0, kLargeBufferSize);
}

// Test copying small pieces of a buffer at different corner case offsets
TEST_P(CopyTests_B2B, SmallCopyInBigBuffer) {
    constexpr uint64_t kEndOffset = kLargeBufferSize - kSmallBufferSize;
    DoTest(kLargeBufferSize, 0, kLargeBufferSize, 0, kSmallBufferSize);
    DoTest(kLargeBufferSize, kEndOffset, kLargeBufferSize, 0, kSmallBufferSize);
    DoTest(kLargeBufferSize, 0, kLargeBufferSize, kEndOffset, kSmallBufferSize);
    DoTest(kLargeBufferSize, kEndOffset, kLargeBufferSize, kEndOffset, kSmallBufferSize);
}

// Test zero-size copies
TEST_P(CopyTests_B2B, ZeroSizedCopy) {
    DoTest(kLargeBufferSize, 0, kLargeBufferSize, 0, 0);
    DoTest(kLargeBufferSize, 0, kLargeBufferSize, kLargeBufferSize, 0);
    DoTest(kLargeBufferSize, kLargeBufferSize, kLargeBufferSize, 0, 0);
    DoTest(kLargeBufferSize, kLargeBufferSize, kLargeBufferSize, kLargeBufferSize, 0);
}

DAWN_INSTANTIATE_TEST(CopyTests_B2B,
                      D3D12Backend(),
                      MetalBackend(),
                      OpenGLBackend(),
                      OpenGLESBackend(),
                      VulkanBackend());

// Test clearing full buffers
TEST_P(ClearBufferTests, FullClear) {
    DoTest(kSmallBufferSize, 0, kSmallBufferSize);
    DoTest(kLargeBufferSize, 0, kLargeBufferSize);
}

// Test clearing small pieces of a buffer at different corner case offsets
TEST_P(ClearBufferTests, SmallClearInBigBuffer) {
    constexpr uint64_t kEndOffset = kLargeBufferSize - kSmallBufferSize;
    DoTest(kLargeBufferSize, 0, kSmallBufferSize);
    DoTest(kLargeBufferSize, kSmallBufferSize, kSmallBufferSize);
    DoTest(kLargeBufferSize, kEndOffset, kSmallBufferSize);
}

// Test zero-size clears
TEST_P(ClearBufferTests, ZeroSizedClear) {
    DoTest(kLargeBufferSize, 0, 0);
    DoTest(kLargeBufferSize, kSmallBufferSize, 0);
    DoTest(kLargeBufferSize, kLargeBufferSize, 0);
}

DAWN_INSTANTIATE_TEST(ClearBufferTests,
                      D3D12Backend(),
                      MetalBackend(),
                      OpenGLBackend(),
                      OpenGLESBackend(),
                      VulkanBackend());
