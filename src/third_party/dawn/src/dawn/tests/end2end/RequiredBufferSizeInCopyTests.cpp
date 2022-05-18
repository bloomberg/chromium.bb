// Copyright 2022 The Dawn Authors
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

#include <vector>

#include "dawn/common/Platform.h"
#include "dawn/tests/DawnTest.h"
#include "dawn/utils/TestUtils.h"
#include "dawn/utils/WGPUHelpers.h"

constexpr static wgpu::Extent3D kCopySize = {1, 1};
constexpr static uint64_t kOffset = 0;
constexpr static uint64_t kBytesPerRow = 256;
constexpr static wgpu::TextureFormat kFormat = wgpu::TextureFormat::RGBA8Unorm;
constexpr static uint32_t kBytesPerBlock = 4;

namespace {
enum class Type { B2TCopy, T2BCopy };

std::ostream& operator<<(std::ostream& o, Type copyType) {
    switch (copyType) {
        case Type::B2TCopy:
            o << "B2TCopy";
            break;
        case Type::T2BCopy:
            o << "T2BCopy";
            break;
    }
    return o;
}

using TextureDimension = wgpu::TextureDimension;
using CopyDepth = uint32_t;
using ExtraRowsPerImage = uint64_t;
DAWN_TEST_PARAM_STRUCT(RequiredBufferSizeInCopyTestsParams,
                       Type,
                       TextureDimension,
                       CopyDepth,
                       ExtraRowsPerImage);
}  // namespace

// Tests in this file are used to expose an error on D3D12 about required minimum buffer size.
// See detailed bug reports at crbug.com/dawn/1278, 1288, 1289.

// When we do B2T or T2B copy from/to a buffer with paddings, it may wrongly calculate
// the required buffer size on D3D12.

// Using the data in this test as an example, in which copySize = {1, 1, 2}, offset = 0, bytesPerRow
// = 256, and rowsPerImage = 2 (there is 1-row padding for every image), and assuming we are copying
// a non-compressed format like rgba8unorm, the required minimum buffer size should be:
//   offset + bytesPerRow * rowsPerImage * (copySize.depthOrArrayLayers - 1)
//     + bytesPerRow * (copySize.height - 1) + bytesPerBlock * copySize.width.
// It is 0 + 256 * 2 * (2 - 1) + 256 * (1 - 1) + 4 * 1 = 516.

// However, the required minimum buffer size on D3D12 (including WARP) is:
//   offset + bytesPerRow * rowsPerImage * (copySize.depthOrArrayLayers - 1)
//     + bytesPerRow * (rowsPerImage - 1) + bytesPerBlock * copySize.width.
// Or
//   offset + bytesPerRow * rowsPerImage * copySize.depthOrArrayLayers
//     + bytesPerBlock * copySize.width - bytesPerRow.
// It is 0 + 256 * 2 * (2 - 1) + 256 * (2 - 1) + 4 * 1 = 772.

// It looks like D3D12 requires unnecessary buffer storage for rowsPerImagePadding in the last
// image. It does respect bytesPerRowPadding in the last row and doesn't require storage for
// that part, though.
//
// Further tests reveal that this incorrect calculation exist only when the texture dimension
// is 3D, and there are rowsPerImage paddings, and there are multiple depth images.

class RequiredBufferSizeInCopyTests
    : public DawnTestWithParams<RequiredBufferSizeInCopyTestsParams> {
  protected:
    void DoTest(const uint64_t bufferSize,
                const wgpu::Extent3D copySize,
                const uint64_t rowsPerImage) {
        wgpu::BufferDescriptor descriptor;
        descriptor.size = bufferSize;
        descriptor.usage = wgpu::BufferUsage::CopySrc | wgpu::BufferUsage::CopyDst;
        wgpu::Buffer buffer = device.CreateBuffer(&descriptor);

        wgpu::TextureDescriptor texDesc = {};
        texDesc.dimension = GetParam().mTextureDimension;
        texDesc.size = copySize;
        texDesc.format = kFormat;
        texDesc.usage = wgpu::TextureUsage::CopyDst | wgpu::TextureUsage::CopySrc;
        wgpu::Texture texture = device.CreateTexture(&texDesc);

        wgpu::ImageCopyTexture imageCopyTexture =
            utils::CreateImageCopyTexture(texture, 0, {0, 0, 0});
        wgpu::ImageCopyBuffer imageCopyBuffer =
            utils::CreateImageCopyBuffer(buffer, kOffset, kBytesPerRow, rowsPerImage);

        // Initialize copied data and set expected data for buffer and texture.
        ASSERT(sizeof(uint32_t) == kBytesPerBlock);
        uint32_t numOfBufferElements = bufferSize / kBytesPerBlock;
        std::vector<uint32_t> data(numOfBufferElements, 1);
        std::vector<uint32_t> expectedBufferData(numOfBufferElements, 0);
        std::vector<uint32_t> expectedTextureData(copySize.depthOrArrayLayers, 0);
        // Initialize the first element on every image to be 0x80808080
        uint64_t imageSize = kBytesPerRow * rowsPerImage;
        ASSERT(bufferSize >= (imageSize * (copySize.depthOrArrayLayers - 1) + kBytesPerBlock));
        uint32_t numOfImageElements = imageSize / kBytesPerBlock;
        for (uint32_t i = 0; i < copySize.depthOrArrayLayers; ++i) {
            data[i * numOfImageElements] = 0x80808080;
            expectedBufferData[i * numOfImageElements] = 0x80808080;
            expectedTextureData[i] = 0x80808080;
        }

        // Do B2T copy or T2B copy
        wgpu::CommandEncoder encoder = this->device.CreateCommandEncoder();
        switch (GetParam().mType) {
            case Type::T2BCopy: {
                wgpu::TextureDataLayout textureDataLayout =
                    utils::CreateTextureDataLayout(kOffset, kBytesPerRow, rowsPerImage);

                queue.WriteTexture(&imageCopyTexture, data.data(), bufferSize, &textureDataLayout,
                                   &copySize);

                encoder.CopyTextureToBuffer(&imageCopyTexture, &imageCopyBuffer, &copySize);
                break;
            }
            case Type::B2TCopy:
                queue.WriteBuffer(buffer, 0, data.data(), bufferSize);
                encoder.CopyBufferToTexture(&imageCopyBuffer, &imageCopyTexture, &copySize);
                break;
        }
        wgpu::CommandBuffer commands = encoder.Finish();
        queue.Submit(1, &commands);

        // Verify the data in buffer (T2B copy) or texture (B2T copy)
        switch (GetParam().mType) {
            case Type::T2BCopy:
                EXPECT_BUFFER_U32_RANGE_EQ(expectedBufferData.data(), buffer, 0, bufferSize / 4);
                break;
            case Type::B2TCopy:
                EXPECT_TEXTURE_EQ(expectedTextureData.data(), texture, {0, 0, 0}, copySize);
                break;
        }
    }
};

// The buffer contains full data on the last image and has storage for all kinds of paddings.
TEST_P(RequiredBufferSizeInCopyTests, AbundantBufferSize) {
    wgpu::Extent3D copySize = kCopySize;
    copySize.depthOrArrayLayers = GetParam().mCopyDepth;
    const uint64_t extraRowsPerImage = GetParam().mExtraRowsPerImage;
    const uint64_t rowsPerImage = extraRowsPerImage + copySize.height;

    uint64_t size = kOffset + kBytesPerRow * rowsPerImage * copySize.depthOrArrayLayers;
    DoTest(size, copySize, rowsPerImage);
}

// The buffer has storage for rowsPerImage paddings on the last image but not bytesPerRow
// paddings on the last row, which is exactly what D3D12 requires. See the comments at the
// beginning of class RequiredBufferSizeInCopyTests for details.
TEST_P(RequiredBufferSizeInCopyTests, BufferSizeOnBoundary) {
    wgpu::Extent3D copySize = kCopySize;
    copySize.depthOrArrayLayers = GetParam().mCopyDepth;
    const uint64_t extraRowsPerImage = GetParam().mExtraRowsPerImage;
    // If there are no rowsPerImage paddings, buffer size required by D3D12 will be exactly the
    // same with the minimum size required by WebGPU spec, which can be covered by tests below
    // at MinimunBufferSize.
    if (extraRowsPerImage > 0) {
        const uint64_t rowsPerImage = extraRowsPerImage + copySize.height;

        uint64_t size = kOffset + kBytesPerRow * rowsPerImage * (copySize.depthOrArrayLayers - 1) +
                        kBytesPerRow * (rowsPerImage - 1) + kBytesPerBlock * copySize.width;
        DoTest(size, copySize, rowsPerImage);

        size -= kBytesPerBlock;
        DoTest(size, copySize, rowsPerImage);
    }
}

// The buffer doesn't have storage for any paddings on the last image. WebGPU spec doesn't require
// storage for these paddings, and the copy operation will never access to these paddings. So it
// should work.
TEST_P(RequiredBufferSizeInCopyTests, MinimumBufferSize) {
    wgpu::Extent3D copySize = kCopySize;
    copySize.depthOrArrayLayers = GetParam().mCopyDepth;
    const uint64_t extraRowsPerImage = GetParam().mExtraRowsPerImage;
    const uint64_t rowsPerImage = extraRowsPerImage + copySize.height;

    uint64_t size =
        kOffset + utils::RequiredBytesInCopy(kBytesPerRow, rowsPerImage, copySize, kFormat);
    DoTest(size, copySize, rowsPerImage);
}

DAWN_INSTANTIATE_TEST_P(
    RequiredBufferSizeInCopyTests,
    {D3D12Backend(), D3D12Backend({"d3d12_split_buffer_texture_copy_for_rows_per_image_paddings"}),
     MetalBackend(), OpenGLBackend(), OpenGLESBackend(), VulkanBackend()},
    {Type::T2BCopy, Type::B2TCopy},
    {wgpu::TextureDimension::e3D, wgpu::TextureDimension::e2D},
    {2u, 1u},
    {1u, 0u});
