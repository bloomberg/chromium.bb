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

#include "tests/DawnTest.h"

#include "common/Constants.h"
#include "common/Math.h"
#include "utils/TestUtils.h"
#include "utils/TextureFormatUtils.h"
#include "utils/WGPUHelpers.h"

class CopyTextureForBrowserTests : public DawnTest {
  protected:
    static constexpr wgpu::TextureFormat kTextureFormat = wgpu::TextureFormat::RGBA8Unorm;

    struct TextureSpec {
        wgpu::Origin3D copyOrigin;
        wgpu::Extent3D textureSize;
        uint32_t level;
    };

    static std::vector<RGBA8> GetExpectedTextureData(const utils::TextureDataCopyLayout& layout) {
        std::vector<RGBA8> textureData(layout.texelBlockCount);
        for (uint32_t layer = 0; layer < layout.mipSize.depth; ++layer) {
            const uint32_t sliceOffset = layout.texelBlocksPerImage * layer;
            for (uint32_t y = 0; y < layout.mipSize.height; ++y) {
                const uint32_t rowOffset = layout.texelBlocksPerRow * y;
                for (uint32_t x = 0; x < layout.mipSize.width; ++x) {
                    textureData[sliceOffset + rowOffset + x] =
                        RGBA8(static_cast<uint8_t>((x + layer * x) % 256),
                              static_cast<uint8_t>((y + layer * y) % 256),
                              static_cast<uint8_t>(x / 256), static_cast<uint8_t>(y / 256));
                }
            }
        }

        return textureData;
    }

    static void PackTextureData(const RGBA8* srcData,
                                uint32_t width,
                                uint32_t height,
                                uint32_t srcTexelsPerRow,
                                RGBA8* dstData,
                                uint32_t dstTexelsPerRow) {
        for (unsigned int y = 0; y < height; ++y) {
            for (unsigned int x = 0; x < width; ++x) {
                unsigned int src = x + y * srcTexelsPerRow;
                unsigned int dst = x + y * dstTexelsPerRow;
                dstData[dst] = srcData[src];
            }
        }
    }

    void DoTest(const TextureSpec& srcSpec,
                const TextureSpec& dstSpec,
                const wgpu::Extent3D& copySize) {
        wgpu::TextureDescriptor srcDescriptor;
        srcDescriptor.size = srcSpec.textureSize;
        srcDescriptor.format = kTextureFormat;
        srcDescriptor.mipLevelCount = srcSpec.level + 1;
        srcDescriptor.usage = wgpu::TextureUsage::CopySrc | wgpu::TextureUsage::CopyDst |
                              wgpu::TextureUsage::Sampled | wgpu::TextureUsage::OutputAttachment;
        wgpu::Texture srcTexture = device.CreateTexture(&srcDescriptor);

        wgpu::Texture dstTexture;
        wgpu::TextureDescriptor dstDescriptor;
        dstDescriptor.size = dstSpec.textureSize;
        dstDescriptor.format = kTextureFormat;
        dstDescriptor.mipLevelCount = dstSpec.level + 1;
        dstDescriptor.usage = wgpu::TextureUsage::CopySrc | wgpu::TextureUsage::CopyDst |
                              wgpu::TextureUsage::OutputAttachment;
        dstTexture = device.CreateTexture(&dstDescriptor);

        wgpu::CommandEncoder encoder = device.CreateCommandEncoder();

        // Use writeTexture to populate the current slice of the texture in
        // `level` mip level for all platforms except OpenGL.
        // TODO(shaobo.yan@intel.com): OpenGL doesn't have 'WriteTexture' implementation.
        const utils::TextureDataCopyLayout copyLayout =
            utils::GetTextureDataCopyLayoutForTexture2DAtLevel(
                kTextureFormat,
                {srcSpec.textureSize.width, srcSpec.textureSize.height, copySize.depth},
                srcSpec.level);

        const std::vector<RGBA8> textureArrayCopyData = GetExpectedTextureData(copyLayout);
        wgpu::TextureCopyView textureCopyView =
            utils::CreateTextureCopyView(srcTexture, srcSpec.level, {0, 0, srcSpec.copyOrigin.z});

        wgpu::TextureDataLayout textureDataLayout;
        textureDataLayout.offset = 0;
        textureDataLayout.bytesPerRow = copyLayout.bytesPerRow;
        textureDataLayout.rowsPerImage = copyLayout.rowsPerImage;

        device.GetDefaultQueue().WriteTexture(&textureCopyView, textureArrayCopyData.data(),
                                              textureArrayCopyData.size() * sizeof(RGBA8),
                                              &textureDataLayout, &copyLayout.mipSize);

        const wgpu::Extent3D copySizePerSlice = {copySize.width, copySize.height, 1};
        // Perform the texture to texture copy
        wgpu::TextureCopyView srcTextureCopyView =
            utils::CreateTextureCopyView(srcTexture, srcSpec.level, srcSpec.copyOrigin);
        wgpu::TextureCopyView dstTextureCopyView =
            utils::CreateTextureCopyView(dstTexture, dstSpec.level, dstSpec.copyOrigin);

        wgpu::CommandBuffer commands = encoder.Finish();
        queue.Submit(1, &commands);

        // Perform a copy here for testing.
        device.GetDefaultQueue().CopyTextureForBrowser(&srcTextureCopyView, &dstTextureCopyView,
                                                       &copySize);

        // Texels in single slice.
        const uint32_t texelCountInCopyRegion = utils::GetTexelCountInCopyRegion(
            copyLayout.bytesPerRow, copyLayout.bytesPerImage / copyLayout.bytesPerRow,
            copySizePerSlice, kTextureFormat);
        std::vector<RGBA8> expected(texelCountInCopyRegion);
        for (uint32_t slice = 0; slice < copySize.depth; ++slice) {
            std::fill(expected.begin(), expected.end(), RGBA8());
            const uint32_t texelIndexOffset = copyLayout.texelBlocksPerImage * slice;
            const uint32_t expectedTexelArrayDataStartIndex =
                texelIndexOffset +
                (srcSpec.copyOrigin.x + srcSpec.copyOrigin.y * copyLayout.texelBlocksPerRow);
            PackTextureData(&textureArrayCopyData[expectedTexelArrayDataStartIndex], copySize.width,
                            copySize.height, copyLayout.texelBlocksPerRow, expected.data(),
                            copySize.width);

            EXPECT_TEXTURE_RGBA8_EQ(expected.data(), dstTexture, dstSpec.copyOrigin.x,
                                    dstSpec.copyOrigin.y, copySize.width, copySize.height,
                                    dstSpec.level, dstSpec.copyOrigin.z + slice)
                << "Texture to Texture copy failed copying region [(" << srcSpec.copyOrigin.x
                << ", " << srcSpec.copyOrigin.y << "), (" << srcSpec.copyOrigin.x + copySize.width
                << ", " << srcSpec.copyOrigin.y + copySize.height << ")) from "
                << srcSpec.textureSize.width << " x " << srcSpec.textureSize.height
                << " texture at mip level " << srcSpec.level << " layer "
                << srcSpec.copyOrigin.z + slice << " to [(" << dstSpec.copyOrigin.x << ", "
                << dstSpec.copyOrigin.y << "), (" << dstSpec.copyOrigin.x + copySize.width << ", "
                << dstSpec.copyOrigin.y + copySize.height << ")) region of "
                << dstSpec.textureSize.width << " x " << dstSpec.textureSize.height
                << " texture at mip level " << dstSpec.level << " layer "
                << dstSpec.copyOrigin.z + slice << std::endl;
        }
    }
};

// Verify CopyTextureForBrowserTests works with internal pipeline.
// The case do copy without any transform.
TEST_P(CopyTextureForBrowserTests, PassthroughCopy) {
    // These tests fails due to crbug.com/tint/63.
    DAWN_SKIP_TEST_IF(IsSwiftshader());
    DAWN_SKIP_TEST_IF(IsVulkan());
    DAWN_SKIP_TEST_IF(IsD3D12() && IsBackendValidationEnabled());

    // OpenGL tests fails due to 'WriteTexture' unimplemented.
    // Related bug : crbug.com/dawn/483
    DAWN_SKIP_TEST_IF(IsOpenGL());

    constexpr uint32_t kWidth = 10;
    constexpr uint32_t kHeight = 1;

    TextureSpec textureSpec;
    textureSpec.copyOrigin = {0, 0, 0};
    textureSpec.level = 0;
    textureSpec.textureSize = {kWidth, kHeight, 1};
    DoTest(textureSpec, textureSpec, {kWidth, kHeight, 1});
}

DAWN_INSTANTIATE_TEST(CopyTextureForBrowserTests,
                      D3D12Backend(),
                      MetalBackend(),
                      OpenGLBackend(),
                      VulkanBackend());
