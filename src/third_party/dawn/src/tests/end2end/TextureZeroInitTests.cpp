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

#include "tests/DawnTest.h"

#include "common/Math.h"
#include "utils/ComboRenderPipelineDescriptor.h"
#include "utils/TestUtils.h"
#include "utils/WGPUHelpers.h"

#define EXPECT_LAZY_CLEAR(N, statement)                                                       \
    do {                                                                                      \
        if (UsesWire()) {                                                                     \
            statement;                                                                        \
        } else {                                                                              \
            size_t lazyClearsBefore = dawn_native::GetLazyClearCountForTesting(device.Get()); \
            statement;                                                                        \
            size_t lazyClearsAfter = dawn_native::GetLazyClearCountForTesting(device.Get());  \
            EXPECT_EQ(N, lazyClearsAfter - lazyClearsBefore);                                 \
        }                                                                                     \
    } while (0)

class TextureZeroInitTest : public DawnTest {
  protected:
    void SetUp() override {
        DawnTest::SetUp();
        DAWN_TEST_UNSUPPORTED_IF(UsesWire());
    }
    wgpu::TextureDescriptor CreateTextureDescriptor(uint32_t mipLevelCount,
                                                    uint32_t arrayLayerCount,
                                                    wgpu::TextureUsage usage,
                                                    wgpu::TextureFormat format) {
        wgpu::TextureDescriptor descriptor;
        descriptor.dimension = wgpu::TextureDimension::e2D;
        descriptor.size.width = kSize;
        descriptor.size.height = kSize;
        descriptor.size.depthOrArrayLayers = arrayLayerCount;
        descriptor.sampleCount = 1;
        descriptor.format = format;
        descriptor.mipLevelCount = mipLevelCount;
        descriptor.usage = usage;
        return descriptor;
    }
    wgpu::TextureViewDescriptor CreateTextureViewDescriptor(
        uint32_t baseMipLevel,
        uint32_t baseArrayLayer,
        wgpu::TextureFormat format = kColorFormat) {
        wgpu::TextureViewDescriptor descriptor;
        descriptor.format = format;
        descriptor.baseArrayLayer = baseArrayLayer;
        descriptor.arrayLayerCount = 1;
        descriptor.baseMipLevel = baseMipLevel;
        descriptor.mipLevelCount = 1;
        descriptor.dimension = wgpu::TextureViewDimension::e2D;
        return descriptor;
    }
    wgpu::RenderPipeline CreatePipelineForTest(float depth = 0.f) {
        utils::ComboRenderPipelineDescriptor pipelineDescriptor;
        pipelineDescriptor.vertex.module = CreateBasicVertexShaderForTest(depth);
        const char* fs = R"(
            ;
            [[stage(fragment)]] fn main() -> [[location(0)]] vec4<f32> {
               return vec4<f32>(1.0, 0.0, 0.0, 1.0);
            }
        )";
        pipelineDescriptor.cFragment.module = utils::CreateShaderModule(device, fs);
        wgpu::DepthStencilState* depthStencil = pipelineDescriptor.EnableDepthStencil();
        depthStencil->depthCompare = wgpu::CompareFunction::Equal;
        depthStencil->stencilFront.compare = wgpu::CompareFunction::Equal;

        return device.CreateRenderPipeline(&pipelineDescriptor);
    }
    wgpu::ShaderModule CreateBasicVertexShaderForTest(float depth = 0.f) {
        std::string source = R"(
            [[stage(vertex)]]
            fn main([[builtin(vertex_index)]] VertexIndex : u32) -> [[builtin(position)]] vec4<f32> {
                var pos = array<vec2<f32>, 6>(
                    vec2<f32>(-1.0, -1.0),
                    vec2<f32>(-1.0,  1.0),
                    vec2<f32>( 1.0, -1.0),
                    vec2<f32>( 1.0,  1.0),
                    vec2<f32>(-1.0,  1.0),
                    vec2<f32>( 1.0, -1.0)
                );
                return vec4<f32>(pos[VertexIndex], )" +
                             std::to_string(depth) + R"(, 1.0);
            })";
        return utils::CreateShaderModule(device, source.c_str());
    }
    wgpu::ShaderModule CreateSampledTextureFragmentShaderForTest() {
        return utils::CreateShaderModule(device, R"(
            [[group(0), binding(0)]] var texture0 : texture_2d<f32>;
            struct FragmentOut {
                [[location(0)]] color : vec4<f32>;
            };
            [[stage(fragment)]]
            fn main([[builtin(position)]] FragCoord : vec4<f32>) -> FragmentOut {
                var output : FragmentOut;
                output.color = textureLoad(texture0, vec2<i32>(FragCoord.xy), 0);
                return output;
            }
        )");
    }

    constexpr static uint32_t kSize = 128;
    constexpr static uint32_t kUnalignedSize = 127;
    // All texture formats used (RGBA8Unorm, Depth24PlusStencil8, and RGBA8Snorm, BC formats)
    // have the same block byte size of 4.
    constexpr static uint32_t kFormatBlockByteSize = 4;
    constexpr static wgpu::TextureFormat kColorFormat = wgpu::TextureFormat::RGBA8Unorm;
    constexpr static wgpu::TextureFormat kDepthStencilFormat =
        wgpu::TextureFormat::Depth24PlusStencil8;
    constexpr static wgpu::TextureFormat kNonrenderableColorFormat =
        wgpu::TextureFormat::RGBA8Snorm;
};

// This tests that the code path of CopyTextureToBuffer clears correctly to Zero after first usage
TEST_P(TextureZeroInitTest, CopyTextureToBufferSource) {
    wgpu::TextureDescriptor descriptor = CreateTextureDescriptor(
        1, 1, wgpu::TextureUsage::RenderAttachment | wgpu::TextureUsage::CopySrc, kColorFormat);
    wgpu::Texture texture = device.CreateTexture(&descriptor);

    // Texture's first usage is in EXPECT_PIXEL_RGBA8_EQ's call to CopyTextureToBuffer
    RGBA8 filledWithZeros(0, 0, 0, 0);
    EXPECT_LAZY_CLEAR(1u, EXPECT_PIXEL_RGBA8_EQ(filledWithZeros, texture, 0, 0));

    // Expect texture subresource initialized to be true
    EXPECT_EQ(true, dawn_native::IsTextureSubresourceInitialized(texture.Get(), 0, 1, 0, 1));
}

// This tests that the code path of CopyTextureToBuffer with multiple texture array layers clears
// correctly to Zero after first usage
TEST_P(TextureZeroInitTest, CopyMultipleTextureArrayLayersToBufferSource) {
    constexpr uint32_t kArrayLayers = 6u;

    const wgpu::TextureDescriptor descriptor = CreateTextureDescriptor(
        1, kArrayLayers, wgpu::TextureUsage::RenderAttachment | wgpu::TextureUsage::CopySrc,
        kColorFormat);
    wgpu::Texture texture = device.CreateTexture(&descriptor);

    const uint32_t bytesPerRow = utils::GetMinimumBytesPerRow(kColorFormat, kSize);
    const uint32_t rowsPerImage = kSize;
    wgpu::BufferDescriptor bufferDescriptor;
    bufferDescriptor.usage = wgpu::BufferUsage::CopySrc | wgpu::BufferUsage::CopyDst;
    bufferDescriptor.size = utils::RequiredBytesInCopy(bytesPerRow, rowsPerImage,
                                                       {kSize, kSize, kArrayLayers}, kColorFormat);
    wgpu::Buffer buffer = device.CreateBuffer(&bufferDescriptor);

    const wgpu::ImageCopyBuffer imageCopyBuffer =
        utils::CreateImageCopyBuffer(buffer, 0, bytesPerRow, kSize);
    const wgpu::ImageCopyTexture imageCopyTexture =
        utils::CreateImageCopyTexture(texture, 0, {0, 0, 0});
    const wgpu::Extent3D copySize = {kSize, kSize, kArrayLayers};

    wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
    encoder.CopyTextureToBuffer(&imageCopyTexture, &imageCopyBuffer, &copySize);
    wgpu::CommandBuffer commandBuffer = encoder.Finish();

    // Expect texture to be lazy initialized.
    EXPECT_LAZY_CLEAR(1u, queue.Submit(1, &commandBuffer));

    // Expect texture subresource initialized to be true
    EXPECT_TRUE(dawn_native::IsTextureSubresourceInitialized(texture.Get(), 0, 1, 0, kArrayLayers));

    const std::vector<RGBA8> kExpectedAllZero(kSize * kSize, {0, 0, 0, 0});
    for (uint32_t layer = 0; layer < kArrayLayers; ++layer) {
        EXPECT_TEXTURE_EQ(kExpectedAllZero.data(), texture, {0, 0, layer}, {kSize, kSize});
    }
}

// Test that non-zero mip level clears subresource to Zero after first use
// This goes through the BeginRenderPass's code path
TEST_P(TextureZeroInitTest, RenderingMipMapClearsToZero) {
    uint32_t baseMipLevel = 2;
    uint32_t levelCount = 4;
    uint32_t baseArrayLayer = 0;
    uint32_t layerCount = 1;

    wgpu::TextureDescriptor descriptor = CreateTextureDescriptor(
        levelCount, layerCount, wgpu::TextureUsage::RenderAttachment | wgpu::TextureUsage::CopySrc,
        kColorFormat);
    wgpu::Texture texture = device.CreateTexture(&descriptor);

    wgpu::TextureViewDescriptor viewDescriptor =
        CreateTextureViewDescriptor(baseMipLevel, baseArrayLayer);
    wgpu::TextureView view = texture.CreateView(&viewDescriptor);

    utils::BasicRenderPass renderPass = utils::BasicRenderPass(kSize, kSize, texture, kColorFormat);

    // Specify loadOp Load. Clear should be used to zero-initialize.
    renderPass.renderPassInfo.cColorAttachments[0].loadOp = wgpu::LoadOp::Load;
    // Specify non-zero clear color. It should still be cleared to zero.
    renderPass.renderPassInfo.cColorAttachments[0].clearColor = {0.5f, 0.5f, 0.5f, 0.5f};
    renderPass.renderPassInfo.cColorAttachments[0].view = view;

    wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
    {
        // Texture's first usage is in BeginRenderPass's call to RecordRenderPass
        wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&renderPass.renderPassInfo);
        pass.EndPass();
    }
    wgpu::CommandBuffer commands = encoder.Finish();
    EXPECT_LAZY_CLEAR(0u, queue.Submit(1, &commands));

    uint32_t mipSize = kSize >> 2;
    std::vector<RGBA8> expected(mipSize * mipSize, {0, 0, 0, 0});

    EXPECT_TEXTURE_EQ(expected.data(), renderPass.color, {0, 0, baseArrayLayer}, {mipSize, mipSize},
                      baseMipLevel);

    // Expect texture subresource initialized to be true
    EXPECT_EQ(true, dawn_native::IsTextureSubresourceInitialized(
                        renderPass.color.Get(), baseMipLevel, 1, baseArrayLayer, 1));
}

// Test that non-zero array layers clears subresource to Zero after first use.
// This goes through the BeginRenderPass's code path
TEST_P(TextureZeroInitTest, RenderingArrayLayerClearsToZero) {
    uint32_t baseMipLevel = 0;
    uint32_t levelCount = 1;
    uint32_t baseArrayLayer = 2;
    uint32_t layerCount = 4;

    wgpu::TextureDescriptor descriptor = CreateTextureDescriptor(
        levelCount, layerCount, wgpu::TextureUsage::RenderAttachment | wgpu::TextureUsage::CopySrc,
        kColorFormat);
    wgpu::Texture texture = device.CreateTexture(&descriptor);

    wgpu::TextureViewDescriptor viewDescriptor =
        CreateTextureViewDescriptor(baseMipLevel, baseArrayLayer);
    wgpu::TextureView view = texture.CreateView(&viewDescriptor);

    utils::BasicRenderPass renderPass = utils::BasicRenderPass(kSize, kSize, texture, kColorFormat);

    // Specify loadOp Load. Clear should be used to zero-initialize.
    renderPass.renderPassInfo.cColorAttachments[0].loadOp = wgpu::LoadOp::Load;
    // Specify non-zero clear color. It should still be cleared to zero.
    renderPass.renderPassInfo.cColorAttachments[0].clearColor = {0.5f, 0.5f, 0.5f, 0.5f};
    renderPass.renderPassInfo.cColorAttachments[0].view = view;

    wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
    {
        wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&renderPass.renderPassInfo);
        pass.EndPass();
    }
    wgpu::CommandBuffer commands = encoder.Finish();
    EXPECT_LAZY_CLEAR(0u, queue.Submit(1, &commands));

    std::vector<RGBA8> expected(kSize * kSize, {0, 0, 0, 0});

    EXPECT_TEXTURE_EQ(expected.data(), renderPass.color, {0, 0, baseArrayLayer}, {kSize, kSize},
                      baseMipLevel);

    // Expect texture subresource initialized to be true
    EXPECT_EQ(true, dawn_native::IsTextureSubresourceInitialized(
                        renderPass.color.Get(), baseMipLevel, 1, baseArrayLayer, 1));
}

// This tests CopyBufferToTexture fully overwrites copy so lazy init is not needed.
TEST_P(TextureZeroInitTest, CopyBufferToTexture) {
    wgpu::TextureDescriptor descriptor =
        CreateTextureDescriptor(4, 1,
                                wgpu::TextureUsage::CopyDst | wgpu::TextureUsage::TextureBinding |
                                    wgpu::TextureUsage::CopySrc,
                                kColorFormat);
    wgpu::Texture texture = device.CreateTexture(&descriptor);

    std::vector<uint8_t> data(kFormatBlockByteSize * kSize * kSize, 100);
    wgpu::Buffer stagingBuffer = utils::CreateBufferFromData(
        device, data.data(), static_cast<uint32_t>(data.size()), wgpu::BufferUsage::CopySrc);

    wgpu::ImageCopyBuffer imageCopyBuffer =
        utils::CreateImageCopyBuffer(stagingBuffer, 0, kSize * sizeof(uint32_t));
    wgpu::ImageCopyTexture imageCopyTexture = utils::CreateImageCopyTexture(texture, 0, {0, 0, 0});
    wgpu::Extent3D copySize = {kSize, kSize, 1};

    wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
    encoder.CopyBufferToTexture(&imageCopyBuffer, &imageCopyTexture, &copySize);
    wgpu::CommandBuffer commands = encoder.Finish();
    EXPECT_LAZY_CLEAR(0u, queue.Submit(1, &commands));

    std::vector<RGBA8> expected(kSize * kSize, {100, 100, 100, 100});

    EXPECT_TEXTURE_EQ(expected.data(), texture, {0, 0}, {kSize, kSize});

    // Expect texture subresource initialized to be true
    EXPECT_EQ(true, dawn_native::IsTextureSubresourceInitialized(texture.Get(), 0, 1, 0, 1));
}

// Test for a copy only to a subset of the subresource, lazy init is necessary to clear the other
// half.
TEST_P(TextureZeroInitTest, CopyBufferToTextureHalf) {
    wgpu::TextureDescriptor descriptor =
        CreateTextureDescriptor(4, 1,
                                wgpu::TextureUsage::CopyDst | wgpu::TextureUsage::TextureBinding |
                                    wgpu::TextureUsage::CopySrc,
                                kColorFormat);
    wgpu::Texture texture = device.CreateTexture(&descriptor);

    std::vector<uint8_t> data(kFormatBlockByteSize * kSize * kSize, 100);
    wgpu::Buffer stagingBuffer = utils::CreateBufferFromData(
        device, data.data(), static_cast<uint32_t>(data.size()), wgpu::BufferUsage::CopySrc);

    wgpu::ImageCopyBuffer imageCopyBuffer =
        utils::CreateImageCopyBuffer(stagingBuffer, 0, kSize * sizeof(uint16_t));
    wgpu::ImageCopyTexture imageCopyTexture = utils::CreateImageCopyTexture(texture, 0, {0, 0, 0});
    wgpu::Extent3D copySize = {kSize / 2, kSize, 1};

    wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
    encoder.CopyBufferToTexture(&imageCopyBuffer, &imageCopyTexture, &copySize);
    wgpu::CommandBuffer commands = encoder.Finish();
    EXPECT_LAZY_CLEAR(1u, queue.Submit(1, &commands));

    std::vector<RGBA8> expected100((kSize / 2) * kSize, {100, 100, 100, 100});
    std::vector<RGBA8> expectedZeros((kSize / 2) * kSize, {0, 0, 0, 0});
    // first half filled with 100, by the buffer data
    EXPECT_TEXTURE_EQ(expected100.data(), texture, {0, 0}, {kSize / 2, kSize});
    // second half should be cleared
    EXPECT_TEXTURE_EQ(expectedZeros.data(), texture, {kSize / 2, 0}, {kSize / 2, kSize});

    // Expect texture subresource initialized to be true
    EXPECT_EQ(true, dawn_native::IsTextureSubresourceInitialized(texture.Get(), 0, 1, 0, 1));
}

// This tests CopyBufferToTexture fully overwrites a range of subresources, so lazy initialization
// is needed for neither the subresources involved in the copy nor the other subresources.
TEST_P(TextureZeroInitTest, CopyBufferToTextureMultipleArrayLayers) {
    wgpu::TextureDescriptor descriptor = CreateTextureDescriptor(
        1, 6, wgpu::TextureUsage::CopyDst | wgpu::TextureUsage::CopySrc, kColorFormat);
    wgpu::Texture texture = device.CreateTexture(&descriptor);

    constexpr uint32_t kBaseArrayLayer = 2u;
    constexpr uint32_t kCopyLayerCount = 3u;
    std::vector<uint8_t> data(kFormatBlockByteSize * kSize * kSize * kCopyLayerCount, 100);
    wgpu::Buffer stagingBuffer = utils::CreateBufferFromData(
        device, data.data(), static_cast<uint32_t>(data.size()), wgpu::BufferUsage::CopySrc);

    const wgpu::ImageCopyBuffer imageCopyBuffer =
        utils::CreateImageCopyBuffer(stagingBuffer, 0, kSize * kFormatBlockByteSize, kSize);
    const wgpu::ImageCopyTexture imageCopyTexture =
        utils::CreateImageCopyTexture(texture, 0, {0, 0, kBaseArrayLayer});
    const wgpu::Extent3D copySize = {kSize, kSize, kCopyLayerCount};

    wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
    encoder.CopyBufferToTexture(&imageCopyBuffer, &imageCopyTexture, &copySize);
    wgpu::CommandBuffer commands = encoder.Finish();

    // The copy overwrites the whole subresources so we don't need to do lazy initialization on
    // them.
    EXPECT_LAZY_CLEAR(0u, queue.Submit(1, &commands));

    // Expect texture subresource initialized to be true
    EXPECT_TRUE(dawn_native::IsTextureSubresourceInitialized(texture.Get(), 0, 1, kBaseArrayLayer,
                                                             kCopyLayerCount));

    const std::vector<RGBA8> expected100(kSize * kSize, {100, 100, 100, 100});
    for (uint32_t layer = kBaseArrayLayer; layer < kBaseArrayLayer + kCopyLayerCount; ++layer) {
        EXPECT_TEXTURE_EQ(expected100.data(), texture, {0, 0, layer}, {kSize, kSize});
    }
}

// This tests CopyTextureToTexture fully overwrites copy so lazy init is not needed.
TEST_P(TextureZeroInitTest, CopyTextureToTexture) {
    wgpu::TextureDescriptor srcDescriptor = CreateTextureDescriptor(
        1, 1, wgpu::TextureUsage::TextureBinding | wgpu::TextureUsage::CopySrc, kColorFormat);
    wgpu::Texture srcTexture = device.CreateTexture(&srcDescriptor);

    wgpu::ImageCopyTexture srcImageCopyTexture =
        utils::CreateImageCopyTexture(srcTexture, 0, {0, 0, 0});

    wgpu::TextureDescriptor dstDescriptor =
        CreateTextureDescriptor(1, 1,
                                wgpu::TextureUsage::RenderAttachment | wgpu::TextureUsage::CopyDst |
                                    wgpu::TextureUsage::CopySrc,
                                kColorFormat);
    wgpu::Texture dstTexture = device.CreateTexture(&dstDescriptor);

    wgpu::ImageCopyTexture dstImageCopyTexture =
        utils::CreateImageCopyTexture(dstTexture, 0, {0, 0, 0});

    wgpu::Extent3D copySize = {kSize, kSize, 1};

    wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
    encoder.CopyTextureToTexture(&srcImageCopyTexture, &dstImageCopyTexture, &copySize);
    wgpu::CommandBuffer commands = encoder.Finish();
    EXPECT_LAZY_CLEAR(1u, queue.Submit(1, &commands));

    std::vector<RGBA8> expected(kSize * kSize, {0, 0, 0, 0});

    EXPECT_TEXTURE_EQ(expected.data(), srcTexture, {0, 0}, {kSize, kSize});
    EXPECT_TEXTURE_EQ(expected.data(), dstTexture, {0, 0}, {kSize, kSize});

    // Expect texture subresource initialized to be true
    EXPECT_EQ(true, dawn_native::IsTextureSubresourceInitialized(srcTexture.Get(), 0, 1, 0, 1));
    EXPECT_EQ(true, dawn_native::IsTextureSubresourceInitialized(dstTexture.Get(), 0, 1, 0, 1));
}

// This Tests the CopyTextureToTexture's copy only to a subset of the subresource, lazy init is
// necessary to clear the other half.
TEST_P(TextureZeroInitTest, CopyTextureToTextureHalf) {
    wgpu::TextureDescriptor srcDescriptor =
        CreateTextureDescriptor(1, 1,
                                wgpu::TextureUsage::TextureBinding | wgpu::TextureUsage::CopySrc |
                                    wgpu::TextureUsage::CopyDst,
                                kColorFormat);
    wgpu::Texture srcTexture = device.CreateTexture(&srcDescriptor);

    // fill srcTexture with 100
    {
        std::vector<uint8_t> data(kFormatBlockByteSize * kSize * kSize, 100);
        wgpu::Buffer stagingBuffer = utils::CreateBufferFromData(
            device, data.data(), static_cast<uint32_t>(data.size()), wgpu::BufferUsage::CopySrc);
        wgpu::ImageCopyBuffer imageCopyBuffer =
            utils::CreateImageCopyBuffer(stagingBuffer, 0, kSize * kFormatBlockByteSize);
        wgpu::ImageCopyTexture imageCopyTexture =
            utils::CreateImageCopyTexture(srcTexture, 0, {0, 0, 0});
        wgpu::Extent3D copySize = {kSize, kSize, 1};
        wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
        encoder.CopyBufferToTexture(&imageCopyBuffer, &imageCopyTexture, &copySize);
        wgpu::CommandBuffer commands = encoder.Finish();
        queue.Submit(1, &commands);
    }

    wgpu::ImageCopyTexture srcImageCopyTexture =
        utils::CreateImageCopyTexture(srcTexture, 0, {0, 0, 0});

    wgpu::TextureDescriptor dstDescriptor =
        CreateTextureDescriptor(1, 1,
                                wgpu::TextureUsage::RenderAttachment | wgpu::TextureUsage::CopyDst |
                                    wgpu::TextureUsage::CopySrc,
                                kColorFormat);
    wgpu::Texture dstTexture = device.CreateTexture(&dstDescriptor);

    wgpu::ImageCopyTexture dstImageCopyTexture =
        utils::CreateImageCopyTexture(dstTexture, 0, {0, 0, 0});
    wgpu::Extent3D copySize = {kSize / 2, kSize, 1};

    wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
    encoder.CopyTextureToTexture(&srcImageCopyTexture, &dstImageCopyTexture, &copySize);
    wgpu::CommandBuffer commands = encoder.Finish();
    EXPECT_LAZY_CLEAR(1u, queue.Submit(1, &commands));

    std::vector<RGBA8> expectedWithZeros((kSize / 2) * kSize, {0, 0, 0, 0});
    std::vector<RGBA8> expectedWith100(kSize * kSize, {100, 100, 100, 100});

    EXPECT_TEXTURE_EQ(expectedWith100.data(), srcTexture, {0, 0}, {kSize, kSize});
    EXPECT_TEXTURE_EQ(expectedWith100.data(), dstTexture, {0, 0}, {kSize / 2, kSize});
    EXPECT_TEXTURE_EQ(expectedWithZeros.data(), dstTexture, {kSize / 2, 0}, {kSize / 2, kSize});

    // Expect texture subresource initialized to be true
    EXPECT_EQ(true, dawn_native::IsTextureSubresourceInitialized(srcTexture.Get(), 0, 1, 0, 1));
    EXPECT_EQ(true, dawn_native::IsTextureSubresourceInitialized(dstTexture.Get(), 0, 1, 0, 1));
}

// This tests the texture with depth attachment and load op load will init depth stencil texture to
// 0s.
TEST_P(TextureZeroInitTest, RenderingLoadingDepth) {
    wgpu::TextureDescriptor srcDescriptor =
        CreateTextureDescriptor(1, 1,
                                wgpu::TextureUsage::CopySrc | wgpu::TextureUsage::CopyDst |
                                    wgpu::TextureUsage::RenderAttachment,
                                kColorFormat);
    wgpu::Texture srcTexture = device.CreateTexture(&srcDescriptor);

    wgpu::TextureDescriptor depthStencilDescriptor = CreateTextureDescriptor(
        1, 1, wgpu::TextureUsage::RenderAttachment | wgpu::TextureUsage::CopySrc,
        kDepthStencilFormat);
    wgpu::Texture depthStencilTexture = device.CreateTexture(&depthStencilDescriptor);

    utils::ComboRenderPassDescriptor renderPassDescriptor({srcTexture.CreateView()},
                                                          depthStencilTexture.CreateView());
    renderPassDescriptor.cDepthStencilAttachmentInfo.depthLoadOp = wgpu::LoadOp::Load;
    // Set clearDepth to non-zero. It should still be cleared to 0 by the loadOp.
    renderPassDescriptor.cDepthStencilAttachmentInfo.clearDepth = 0.5f;
    renderPassDescriptor.cDepthStencilAttachmentInfo.stencilLoadOp = wgpu::LoadOp::Clear;
    renderPassDescriptor.cDepthStencilAttachmentInfo.clearStencil = 0;
    renderPassDescriptor.cDepthStencilAttachmentInfo.depthStoreOp = wgpu::StoreOp::Store;
    renderPassDescriptor.cDepthStencilAttachmentInfo.stencilStoreOp = wgpu::StoreOp::Store;

    wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
    auto pass = encoder.BeginRenderPass(&renderPassDescriptor);
    pass.SetPipeline(CreatePipelineForTest());
    pass.Draw(6);
    pass.EndPass();
    wgpu::CommandBuffer commandBuffer = encoder.Finish();
    // Expect 0 lazy clears, depth stencil texture will clear using loadop
    EXPECT_LAZY_CLEAR(0u, queue.Submit(1, &commandBuffer));

    // Expect the texture to be red because depth test passed.
    std::vector<RGBA8> expected(kSize * kSize, {255, 0, 0, 255});
    EXPECT_TEXTURE_EQ(expected.data(), srcTexture, {0, 0}, {kSize, kSize});

    // Expect texture subresource initialized to be true
    EXPECT_EQ(true, dawn_native::IsTextureSubresourceInitialized(srcTexture.Get(), 0, 1, 0, 1));
}

// This tests the texture with stencil attachment and load op load will init depth stencil texture
// to 0s.
TEST_P(TextureZeroInitTest, RenderingLoadingStencil) {
    wgpu::TextureDescriptor srcDescriptor =
        CreateTextureDescriptor(1, 1,
                                wgpu::TextureUsage::CopySrc | wgpu::TextureUsage::CopyDst |
                                    wgpu::TextureUsage::RenderAttachment,
                                kColorFormat);
    wgpu::Texture srcTexture = device.CreateTexture(&srcDescriptor);

    wgpu::TextureDescriptor depthStencilDescriptor = CreateTextureDescriptor(
        1, 1, wgpu::TextureUsage::RenderAttachment | wgpu::TextureUsage::CopySrc,
        kDepthStencilFormat);
    wgpu::Texture depthStencilTexture = device.CreateTexture(&depthStencilDescriptor);

    utils::ComboRenderPassDescriptor renderPassDescriptor({srcTexture.CreateView()},
                                                          depthStencilTexture.CreateView());
    renderPassDescriptor.cDepthStencilAttachmentInfo.depthLoadOp = wgpu::LoadOp::Clear;
    renderPassDescriptor.cDepthStencilAttachmentInfo.clearDepth = 0.0f;
    renderPassDescriptor.cDepthStencilAttachmentInfo.stencilLoadOp = wgpu::LoadOp::Load;
    // Set clearStencil to non-zero. It should still be cleared to 0 by the loadOp.
    renderPassDescriptor.cDepthStencilAttachmentInfo.clearStencil = 2;
    renderPassDescriptor.cDepthStencilAttachmentInfo.depthStoreOp = wgpu::StoreOp::Store;
    renderPassDescriptor.cDepthStencilAttachmentInfo.stencilStoreOp = wgpu::StoreOp::Store;

    wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
    auto pass = encoder.BeginRenderPass(&renderPassDescriptor);
    pass.SetPipeline(CreatePipelineForTest());
    pass.Draw(6);
    pass.EndPass();
    wgpu::CommandBuffer commandBuffer = encoder.Finish();
    // Expect 0 lazy clears, depth stencil texture will clear using loadop
    EXPECT_LAZY_CLEAR(0u, queue.Submit(1, &commandBuffer));

    // Expect the texture to be red because stencil test passed.
    std::vector<RGBA8> expected(kSize * kSize, {255, 0, 0, 255});
    EXPECT_TEXTURE_EQ(expected.data(), srcTexture, {0, 0}, {kSize, kSize});

    // Expect texture subresource initialized to be true
    EXPECT_EQ(true, dawn_native::IsTextureSubresourceInitialized(srcTexture.Get(), 0, 1, 0, 1));
}

// This tests the texture with depth stencil attachment and load op load will init depth stencil
// texture to 0s.
TEST_P(TextureZeroInitTest, RenderingLoadingDepthStencil) {
    wgpu::TextureDescriptor srcDescriptor =
        CreateTextureDescriptor(1, 1,
                                wgpu::TextureUsage::CopySrc | wgpu::TextureUsage::CopyDst |
                                    wgpu::TextureUsage::RenderAttachment,
                                kColorFormat);
    wgpu::Texture srcTexture = device.CreateTexture(&srcDescriptor);

    wgpu::TextureDescriptor depthStencilDescriptor = CreateTextureDescriptor(
        1, 1, wgpu::TextureUsage::RenderAttachment | wgpu::TextureUsage::CopySrc,
        kDepthStencilFormat);
    wgpu::Texture depthStencilTexture = device.CreateTexture(&depthStencilDescriptor);

    utils::ComboRenderPassDescriptor renderPassDescriptor({srcTexture.CreateView()},
                                                          depthStencilTexture.CreateView());
    renderPassDescriptor.cDepthStencilAttachmentInfo.depthLoadOp = wgpu::LoadOp::Load;
    renderPassDescriptor.cDepthStencilAttachmentInfo.stencilLoadOp = wgpu::LoadOp::Load;
    renderPassDescriptor.cDepthStencilAttachmentInfo.depthStoreOp = wgpu::StoreOp::Store;
    renderPassDescriptor.cDepthStencilAttachmentInfo.stencilStoreOp = wgpu::StoreOp::Store;

    wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
    auto pass = encoder.BeginRenderPass(&renderPassDescriptor);
    pass.SetPipeline(CreatePipelineForTest());
    pass.Draw(6);
    pass.EndPass();
    wgpu::CommandBuffer commandBuffer = encoder.Finish();
    // Expect 0 lazy clears, depth stencil texture will clear using loadop
    EXPECT_LAZY_CLEAR(0u, queue.Submit(1, &commandBuffer));

    // Expect the texture to be red because both depth and stencil tests passed.
    std::vector<RGBA8> expected(kSize * kSize, {255, 0, 0, 255});
    EXPECT_TEXTURE_EQ(expected.data(), srcTexture, {0, 0}, {kSize, kSize});

    // Expect texture subresource initialized to be true
    EXPECT_EQ(true, dawn_native::IsTextureSubresourceInitialized(srcTexture.Get(), 0, 1, 0, 1));
}

// Test that clear state is tracked independently for depth/stencil textures.
TEST_P(TextureZeroInitTest, IndependentDepthStencilLoadAfterDiscard) {
    // TODO(crbug.com/dawn/704): Readback after clear via stencil copy does not work
    // on some Intel drivers.
    DAWN_SUPPRESS_TEST_IF(IsMetal() && IsIntel());

    // TODO(crbug.com/dawn/1151): The test started failing on Wintel Vulkan when Discard was
    // implemented for the Vulkan backend.
    DAWN_SUPPRESS_TEST_IF(IsVulkan() && IsWindows() && IsIntel());

    wgpu::TextureDescriptor depthStencilDescriptor = CreateTextureDescriptor(
        1, 1, wgpu::TextureUsage::RenderAttachment | wgpu::TextureUsage::CopySrc,
        kDepthStencilFormat);
    wgpu::Texture depthStencilTexture = device.CreateTexture(&depthStencilDescriptor);

    // Uninitialize only depth
    {
        // Clear the stencil to 2 and discard the depth
        {
            utils::ComboRenderPassDescriptor renderPassDescriptor({},
                                                                  depthStencilTexture.CreateView());
            renderPassDescriptor.cDepthStencilAttachmentInfo.depthStoreOp = wgpu::StoreOp::Discard;
            renderPassDescriptor.cDepthStencilAttachmentInfo.clearStencil = 2;
            renderPassDescriptor.cDepthStencilAttachmentInfo.stencilStoreOp = wgpu::StoreOp::Store;

            wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
            auto pass = encoder.BeginRenderPass(&renderPassDescriptor);
            pass.EndPass();
            wgpu::CommandBuffer commandBuffer = encoder.Finish();
            EXPECT_LAZY_CLEAR(0u, queue.Submit(1, &commandBuffer));
        }

        // "all" subresources are not initialized; Depth is not initialized
        EXPECT_EQ(false, dawn_native::IsTextureSubresourceInitialized(
                             depthStencilTexture.Get(), 0, 1, 0, 1, WGPUTextureAspect_All));
        EXPECT_EQ(false, dawn_native::IsTextureSubresourceInitialized(
                             depthStencilTexture.Get(), 0, 1, 0, 1, WGPUTextureAspect_DepthOnly));
        EXPECT_EQ(true, dawn_native::IsTextureSubresourceInitialized(
                            depthStencilTexture.Get(), 0, 1, 0, 1, WGPUTextureAspect_StencilOnly));

        // Now load both depth and stencil. Depth should be cleared and stencil should stay the same
        // at 2.
        {
            wgpu::TextureDescriptor colorDescriptor =
                CreateTextureDescriptor(1, 1,
                                        wgpu::TextureUsage::CopySrc | wgpu::TextureUsage::CopyDst |
                                            wgpu::TextureUsage::RenderAttachment,
                                        kColorFormat);
            wgpu::Texture colorTexture = device.CreateTexture(&colorDescriptor);

            utils::ComboRenderPassDescriptor renderPassDescriptor({colorTexture.CreateView()},
                                                                  depthStencilTexture.CreateView());
            renderPassDescriptor.cDepthStencilAttachmentInfo.depthLoadOp = wgpu::LoadOp::Load;
            renderPassDescriptor.cDepthStencilAttachmentInfo.stencilLoadOp = wgpu::LoadOp::Load;

            wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
            auto pass = encoder.BeginRenderPass(&renderPassDescriptor);
            pass.SetPipeline(CreatePipelineForTest());
            pass.SetStencilReference(2);
            pass.Draw(6);
            pass.EndPass();
            wgpu::CommandBuffer commandBuffer = encoder.Finish();
            // No lazy clear because depth will be cleared with a loadOp
            EXPECT_LAZY_CLEAR(0u, queue.Submit(1, &commandBuffer));

            // Expect the texture to be red because the depth and stencil tests passed. Depth was 0
            // and stencil was 2.
            std::vector<RGBA8> expected(kSize * kSize, {255, 0, 0, 255});
            EXPECT_TEXTURE_EQ(expected.data(), colorTexture, {0, 0}, {kSize, kSize});
        }

        // Everything is initialized now
        EXPECT_EQ(true, dawn_native::IsTextureSubresourceInitialized(
                            depthStencilTexture.Get(), 0, 1, 0, 1, WGPUTextureAspect_All));
        EXPECT_EQ(true, dawn_native::IsTextureSubresourceInitialized(
                            depthStencilTexture.Get(), 0, 1, 0, 1, WGPUTextureAspect_DepthOnly));
        EXPECT_EQ(true, dawn_native::IsTextureSubresourceInitialized(
                            depthStencilTexture.Get(), 0, 1, 0, 1, WGPUTextureAspect_StencilOnly));

        // TODO(crbug.com/dawn/439): Implement stencil copies on other platforms
        if (IsMetal() || IsVulkan() || IsD3D12()) {
            // Check by copy that the stencil data is 2.
            std::vector<uint8_t> expected(kSize * kSize, 2);
            EXPECT_LAZY_CLEAR(
                0u, EXPECT_TEXTURE_EQ(expected.data(), depthStencilTexture, {0, 0}, {kSize, kSize},
                                      0, wgpu::TextureAspect::StencilOnly));
        }
    }

    // Uninitialize only stencil
    {
        // Clear the depth to 0.7 and discard the stencil.
        {
            utils::ComboRenderPassDescriptor renderPassDescriptor({},
                                                                  depthStencilTexture.CreateView());
            renderPassDescriptor.cDepthStencilAttachmentInfo.clearDepth = 0.7;
            renderPassDescriptor.cDepthStencilAttachmentInfo.depthStoreOp = wgpu::StoreOp::Store;
            renderPassDescriptor.cDepthStencilAttachmentInfo.stencilStoreOp =
                wgpu::StoreOp::Discard;

            wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
            auto pass = encoder.BeginRenderPass(&renderPassDescriptor);
            pass.EndPass();
            wgpu::CommandBuffer commandBuffer = encoder.Finish();
            EXPECT_LAZY_CLEAR(0u, queue.Submit(1, &commandBuffer));
        }

        // "all" subresources are not initialized; Stencil is not initialized
        EXPECT_EQ(false, dawn_native::IsTextureSubresourceInitialized(
                             depthStencilTexture.Get(), 0, 1, 0, 1, WGPUTextureAspect_All));
        EXPECT_EQ(true, dawn_native::IsTextureSubresourceInitialized(
                            depthStencilTexture.Get(), 0, 1, 0, 1, WGPUTextureAspect_DepthOnly));
        EXPECT_EQ(false, dawn_native::IsTextureSubresourceInitialized(
                             depthStencilTexture.Get(), 0, 1, 0, 1, WGPUTextureAspect_StencilOnly));

        // Now load both depth and stencil. Stencil should be cleared and depth should stay the same
        // at 0.7.
        {
            wgpu::TextureDescriptor colorDescriptor =
                CreateTextureDescriptor(1, 1,
                                        wgpu::TextureUsage::CopySrc | wgpu::TextureUsage::CopyDst |
                                            wgpu::TextureUsage::RenderAttachment,
                                        kColorFormat);
            wgpu::Texture colorTexture = device.CreateTexture(&colorDescriptor);

            utils::ComboRenderPassDescriptor renderPassDescriptor({colorTexture.CreateView()},
                                                                  depthStencilTexture.CreateView());
            renderPassDescriptor.cDepthStencilAttachmentInfo.depthLoadOp = wgpu::LoadOp::Load;
            renderPassDescriptor.cDepthStencilAttachmentInfo.stencilLoadOp = wgpu::LoadOp::Load;

            wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
            auto pass = encoder.BeginRenderPass(&renderPassDescriptor);
            pass.SetPipeline(CreatePipelineForTest(0.7));
            pass.Draw(6);
            pass.EndPass();
            wgpu::CommandBuffer commandBuffer = encoder.Finish();
            // No lazy clear because stencil will clear using a loadOp.
            EXPECT_LAZY_CLEAR(0u, queue.Submit(1, &commandBuffer));

            // Expect the texture to be red because both the depth a stencil tests passed.
            // Depth was 0.7 and stencil was 0
            std::vector<RGBA8> expected(kSize * kSize, {255, 0, 0, 255});
            EXPECT_TEXTURE_EQ(expected.data(), colorTexture, {0, 0}, {kSize, kSize});
        }

        // Everything is initialized now
        EXPECT_EQ(true, dawn_native::IsTextureSubresourceInitialized(
                            depthStencilTexture.Get(), 0, 1, 0, 1, WGPUTextureAspect_All));
        EXPECT_EQ(true, dawn_native::IsTextureSubresourceInitialized(
                            depthStencilTexture.Get(), 0, 1, 0, 1, WGPUTextureAspect_DepthOnly));
        EXPECT_EQ(true, dawn_native::IsTextureSubresourceInitialized(
                            depthStencilTexture.Get(), 0, 1, 0, 1, WGPUTextureAspect_StencilOnly));

        // TODO(crbug.com/dawn/439): Implement stencil copies on other platforms
        if (IsMetal() || IsVulkan() || IsD3D12()) {
            // Check by copy that the stencil data is 0.
            std::vector<uint8_t> expected(kSize * kSize, 0);
            EXPECT_LAZY_CLEAR(
                0u, EXPECT_TEXTURE_EQ(expected.data(), depthStencilTexture, {0, 0}, {kSize, kSize},
                                      0, wgpu::TextureAspect::StencilOnly));
        }
    }
}

// Test that clear state is tracked independently for depth/stencil textures.
// Lazy clear of the stencil aspect via copy should not touch depth.
TEST_P(TextureZeroInitTest, IndependentDepthStencilCopyAfterDiscard) {
    // TODO(crbug.com/dawn/439): Implement stencil copies on other platforms
    DAWN_SUPPRESS_TEST_IF(!(IsMetal() || IsVulkan() || IsD3D12()));

    // TODO(enga): Figure out why this fails on Metal Intel.
    DAWN_SUPPRESS_TEST_IF(IsMetal() && IsIntel());

    wgpu::TextureDescriptor depthStencilDescriptor = CreateTextureDescriptor(
        1, 1, wgpu::TextureUsage::RenderAttachment | wgpu::TextureUsage::CopySrc,
        kDepthStencilFormat);
    wgpu::Texture depthStencilTexture = device.CreateTexture(&depthStencilDescriptor);

    // Clear the depth to 0.3 and discard the stencil.
    {
        utils::ComboRenderPassDescriptor renderPassDescriptor({}, depthStencilTexture.CreateView());
        renderPassDescriptor.cDepthStencilAttachmentInfo.clearDepth = 0.3;
        renderPassDescriptor.cDepthStencilAttachmentInfo.depthStoreOp = wgpu::StoreOp::Store;
        renderPassDescriptor.cDepthStencilAttachmentInfo.stencilStoreOp = wgpu::StoreOp::Discard;

        wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
        auto pass = encoder.BeginRenderPass(&renderPassDescriptor);
        pass.EndPass();
        wgpu::CommandBuffer commandBuffer = encoder.Finish();
        EXPECT_LAZY_CLEAR(0u, queue.Submit(1, &commandBuffer));
    }

    // "all" subresources are not initialized; Stencil is not initialized
    EXPECT_EQ(false, dawn_native::IsTextureSubresourceInitialized(depthStencilTexture.Get(), 0, 1,
                                                                  0, 1, WGPUTextureAspect_All));
    EXPECT_EQ(true, dawn_native::IsTextureSubresourceInitialized(depthStencilTexture.Get(), 0, 1, 0,
                                                                 1, WGPUTextureAspect_DepthOnly));
    EXPECT_EQ(false, dawn_native::IsTextureSubresourceInitialized(
                         depthStencilTexture.Get(), 0, 1, 0, 1, WGPUTextureAspect_StencilOnly));

    // Check by copy that the stencil data is lazily cleared to 0.
    std::vector<uint8_t> expected(kSize * kSize, 0);
    EXPECT_LAZY_CLEAR(1u, EXPECT_TEXTURE_EQ(expected.data(), depthStencilTexture, {0, 0},
                                            {kSize, kSize}, 0, wgpu::TextureAspect::StencilOnly));

    // Everything is initialized now
    EXPECT_EQ(true, dawn_native::IsTextureSubresourceInitialized(depthStencilTexture.Get(), 0, 1, 0,
                                                                 1, WGPUTextureAspect_All));
    EXPECT_EQ(true, dawn_native::IsTextureSubresourceInitialized(depthStencilTexture.Get(), 0, 1, 0,
                                                                 1, WGPUTextureAspect_DepthOnly));
    EXPECT_EQ(true, dawn_native::IsTextureSubresourceInitialized(depthStencilTexture.Get(), 0, 1, 0,
                                                                 1, WGPUTextureAspect_StencilOnly));

    // Now load both depth and stencil. Stencil should be cleared and depth should stay the same
    // at 0.3.
    {
        wgpu::TextureDescriptor colorDescriptor =
            CreateTextureDescriptor(1, 1,
                                    wgpu::TextureUsage::CopySrc | wgpu::TextureUsage::CopyDst |
                                        wgpu::TextureUsage::RenderAttachment,
                                    kColorFormat);
        wgpu::Texture colorTexture = device.CreateTexture(&colorDescriptor);

        utils::ComboRenderPassDescriptor renderPassDescriptor({colorTexture.CreateView()},
                                                              depthStencilTexture.CreateView());
        renderPassDescriptor.cDepthStencilAttachmentInfo.depthLoadOp = wgpu::LoadOp::Load;
        renderPassDescriptor.cDepthStencilAttachmentInfo.stencilLoadOp = wgpu::LoadOp::Load;

        wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
        auto pass = encoder.BeginRenderPass(&renderPassDescriptor);
        pass.SetPipeline(CreatePipelineForTest(0.3));
        pass.Draw(6);
        pass.EndPass();
        wgpu::CommandBuffer commandBuffer = encoder.Finish();
        // No lazy clear because stencil will clear using a loadOp.
        EXPECT_LAZY_CLEAR(0u, queue.Submit(1, &commandBuffer));

        // Expect the texture to be red because both the depth a stencil tests passed.
        // Depth was 0.3 and stencil was 0
        std::vector<RGBA8> expected(kSize * kSize, {255, 0, 0, 255});
        EXPECT_TEXTURE_EQ(expected.data(), colorTexture, {0, 0}, {kSize, kSize});
    }
}

// This tests the color attachments clear to 0s
TEST_P(TextureZeroInitTest, ColorAttachmentsClear) {
    wgpu::TextureDescriptor descriptor = CreateTextureDescriptor(
        1, 1, wgpu::TextureUsage::RenderAttachment | wgpu::TextureUsage::CopySrc, kColorFormat);
    wgpu::Texture texture = device.CreateTexture(&descriptor);
    utils::BasicRenderPass renderPass = utils::BasicRenderPass(kSize, kSize, texture, kColorFormat);
    renderPass.renderPassInfo.cColorAttachments[0].loadOp = wgpu::LoadOp::Load;

    wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
    wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&renderPass.renderPassInfo);
    pass.EndPass();

    wgpu::CommandBuffer commands = encoder.Finish();
    EXPECT_LAZY_CLEAR(0u, queue.Submit(1, &commands));

    std::vector<RGBA8> expected(kSize * kSize, {0, 0, 0, 0});
    EXPECT_TEXTURE_EQ(expected.data(), renderPass.color, {0, 0}, {kSize, kSize});

    // Expect texture subresource initialized to be true
    EXPECT_EQ(true,
              dawn_native::IsTextureSubresourceInitialized(renderPass.color.Get(), 0, 1, 0, 1));
}

// This tests the clearing of sampled textures in render pass
TEST_P(TextureZeroInitTest, RenderPassSampledTextureClear) {
    // Create needed resources
    wgpu::TextureDescriptor descriptor =
        CreateTextureDescriptor(1, 1, wgpu::TextureUsage::TextureBinding, kColorFormat);
    wgpu::Texture texture = device.CreateTexture(&descriptor);

    wgpu::TextureDescriptor renderTextureDescriptor = CreateTextureDescriptor(
        1, 1, wgpu::TextureUsage::CopySrc | wgpu::TextureUsage::RenderAttachment, kColorFormat);
    wgpu::Texture renderTexture = device.CreateTexture(&renderTextureDescriptor);

    // Create render pipeline
    utils::ComboRenderPipelineDescriptor renderPipelineDescriptor;
    renderPipelineDescriptor.cTargets[0].format = kColorFormat;
    renderPipelineDescriptor.vertex.module = CreateBasicVertexShaderForTest();
    renderPipelineDescriptor.cFragment.module = CreateSampledTextureFragmentShaderForTest();
    wgpu::RenderPipeline renderPipeline = device.CreateRenderPipeline(&renderPipelineDescriptor);

    // Create bindgroup
    wgpu::BindGroup bindGroup = utils::MakeBindGroup(device, renderPipeline.GetBindGroupLayout(0),
                                                     {{0, texture.CreateView()}});

    // Encode pass and submit
    wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
    utils::ComboRenderPassDescriptor renderPassDesc({renderTexture.CreateView()});
    renderPassDesc.cColorAttachments[0].clearColor = {1.0, 1.0, 1.0, 1.0};
    renderPassDesc.cColorAttachments[0].loadOp = wgpu::LoadOp::Clear;
    wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&renderPassDesc);
    pass.SetPipeline(renderPipeline);
    pass.SetBindGroup(0, bindGroup);
    pass.Draw(6);
    pass.EndPass();
    wgpu::CommandBuffer commands = encoder.Finish();
    // Expect 1 lazy clear for sampled texture
    EXPECT_LAZY_CLEAR(1u, queue.Submit(1, &commands));

    // Expect the rendered texture to be cleared
    std::vector<RGBA8> expectedWithZeros(kSize * kSize, {0, 0, 0, 0});
    EXPECT_TEXTURE_EQ(expectedWithZeros.data(), renderTexture, {0, 0}, {kSize, kSize});

    // Expect texture subresource initialized to be true
    EXPECT_EQ(true, dawn_native::IsTextureSubresourceInitialized(renderTexture.Get(), 0, 1, 0, 1));
}

// This is a regression test for a bug where a texture wouldn't get clear for a pass if at least
// one of its subresources was used as an attachment. It tests that if a texture is used as both
// sampled and attachment (with LoadOp::Clear so the lazy clear can be skipped) then the sampled
// subresource is correctly cleared.
TEST_P(TextureZeroInitTest, TextureBothSampledAndAttachmentClear) {
    // TODO(crbug.com/dawn/593): This test uses glTextureView() which is not supported on OpenGL ES.
    DAWN_TEST_UNSUPPORTED_IF(IsOpenGLES());

    // Create a 2D array texture, layer 0 will be used as attachment, layer 1 as sampled.
    wgpu::TextureDescriptor texDesc;
    texDesc.usage = wgpu::TextureUsage::TextureBinding | wgpu::TextureUsage::RenderAttachment |
                    wgpu::TextureUsage::CopySrc;
    texDesc.size = {1, 1, 2};
    texDesc.format = wgpu::TextureFormat::RGBA8Unorm;
    wgpu::Texture texture = device.CreateTexture(&texDesc);

    wgpu::TextureViewDescriptor viewDesc;
    viewDesc.dimension = wgpu::TextureViewDimension::e2D;
    viewDesc.arrayLayerCount = 1;

    viewDesc.baseArrayLayer = 0;
    wgpu::TextureView attachmentView = texture.CreateView(&viewDesc);

    viewDesc.baseArrayLayer = 1;
    wgpu::TextureView sampleView = texture.CreateView(&viewDesc);

    // Create render pipeline
    utils::ComboRenderPipelineDescriptor renderPipelineDescriptor;
    renderPipelineDescriptor.cTargets[0].format = wgpu::TextureFormat::RGBA8Unorm;
    renderPipelineDescriptor.vertex.module = CreateBasicVertexShaderForTest();
    renderPipelineDescriptor.cFragment.module = CreateSampledTextureFragmentShaderForTest();
    wgpu::RenderPipeline renderPipeline = device.CreateRenderPipeline(&renderPipelineDescriptor);

    wgpu::BindGroup bindGroup =
        utils::MakeBindGroup(device, renderPipeline.GetBindGroupLayout(0), {{0, sampleView}});

    // Encode pass and submit
    wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
    utils::ComboRenderPassDescriptor renderPassDesc({attachmentView});
    renderPassDesc.cColorAttachments[0].clearColor = {1.0, 1.0, 1.0, 1.0};
    renderPassDesc.cColorAttachments[0].loadOp = wgpu::LoadOp::Clear;
    wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&renderPassDesc);
    pass.SetPipeline(renderPipeline);
    pass.SetBindGroup(0, bindGroup);
    pass.Draw(6);
    pass.EndPass();
    wgpu::CommandBuffer commands = encoder.Finish();

    // Expect the lazy clear for the sampled subresource.
    EXPECT_LAZY_CLEAR(1u, queue.Submit(1, &commands));

    // Expect both subresources to be zero: the sampled one with lazy-clearing and the attachment
    // because it sampled the lazy-cleared sampled subresource.
    EXPECT_TEXTURE_EQ(&RGBA8::kZero, texture, {0, 0, 0}, {1, 1});
    EXPECT_TEXTURE_EQ(&RGBA8::kZero, texture, {0, 0, 1}, {1, 1});

    // The whole texture is now initialized.
    EXPECT_EQ(true, dawn_native::IsTextureSubresourceInitialized(texture.Get(), 0, 1, 0, 2));
}

// This tests the clearing of sampled textures during compute pass
TEST_P(TextureZeroInitTest, ComputePassSampledTextureClear) {
    // Create needed resources
    wgpu::TextureDescriptor descriptor =
        CreateTextureDescriptor(1, 1, wgpu::TextureUsage::TextureBinding, kColorFormat);
    descriptor.size.width = 1;
    descriptor.size.height = 1;
    wgpu::Texture texture = device.CreateTexture(&descriptor);

    uint32_t bufferSize = kFormatBlockByteSize * sizeof(uint32_t);
    wgpu::BufferDescriptor bufferDescriptor;
    bufferDescriptor.size = bufferSize;
    bufferDescriptor.usage =
        wgpu::BufferUsage::CopySrc | wgpu::BufferUsage::Storage | wgpu::BufferUsage::CopyDst;
    wgpu::Buffer bufferTex = device.CreateBuffer(&bufferDescriptor);
    // Add data to buffer to ensure it is initialized
    uint32_t data = 100;
    queue.WriteBuffer(bufferTex, 0, &data, sizeof(data));

    wgpu::Sampler sampler = device.CreateSampler();

    // Create compute pipeline
    wgpu::ComputePipelineDescriptor computePipelineDescriptor;
    wgpu::ProgrammableStageDescriptor compute;
    const char* cs = R"(
        [[group(0), binding(0)]] var tex : texture_2d<f32>;
        [[block]] struct Result {
            value : vec4<f32>;
        };
        [[group(0), binding(1)]] var<storage, read_write> result : Result;
        [[stage(compute), workgroup_size(1)]] fn main() {
           result.value = textureLoad(tex, vec2<i32>(0,0), 0);
        }
    )";
    computePipelineDescriptor.compute.module = utils::CreateShaderModule(device, cs);
    computePipelineDescriptor.compute.entryPoint = "main";
    wgpu::ComputePipeline computePipeline =
        device.CreateComputePipeline(&computePipelineDescriptor);

    // Create bindgroup
    wgpu::BindGroup bindGroup =
        utils::MakeBindGroup(device, computePipeline.GetBindGroupLayout(0),
                             {{0, texture.CreateView()}, {1, bufferTex, 0, bufferSize}});

    // Encode the pass and submit
    wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
    wgpu::ComputePassEncoder pass = encoder.BeginComputePass();
    pass.SetPipeline(computePipeline);
    pass.SetBindGroup(0, bindGroup);
    pass.Dispatch(1);
    pass.EndPass();
    wgpu::CommandBuffer commands = encoder.Finish();
    EXPECT_LAZY_CLEAR(1u, queue.Submit(1, &commands));

    // Expect the buffer to be zeroed out by the compute pass
    std::vector<uint32_t> expectedWithZeros(bufferSize, 0);
    EXPECT_BUFFER_U32_RANGE_EQ(expectedWithZeros.data(), bufferTex, 0, kFormatBlockByteSize);

    // Expect texture subresource initialized to be true
    EXPECT_EQ(true, dawn_native::IsTextureSubresourceInitialized(texture.Get(), 0, 1, 0, 1));
}

// This tests that the code path of CopyTextureToBuffer clears correctly for non-renderable textures
TEST_P(TextureZeroInitTest, NonRenderableTextureClear) {
    // TODO(crbug.com/dawn/667): Work around the fact that some platforms do not support reading
    // from Snorm textures.
    DAWN_TEST_UNSUPPORTED_IF(HasToggleEnabled("disable_snorm_read"));

    wgpu::TextureDescriptor descriptor =
        CreateTextureDescriptor(1, 1, wgpu::TextureUsage::CopySrc, kNonrenderableColorFormat);
    wgpu::Texture texture = device.CreateTexture(&descriptor);

    // Set buffer with dirty data so we know it is cleared by the lazy cleared texture copy
    uint32_t bytesPerRow = Align(kSize * kFormatBlockByteSize, kTextureBytesPerRowAlignment);
    uint32_t bufferSize = bytesPerRow * kSize;
    std::vector<uint8_t> data(bufferSize, 100);
    wgpu::Buffer bufferDst = utils::CreateBufferFromData(
        device, data.data(), static_cast<uint32_t>(data.size()), wgpu::BufferUsage::CopySrc);

    wgpu::ImageCopyBuffer imageCopyBuffer = utils::CreateImageCopyBuffer(bufferDst, 0, bytesPerRow);
    wgpu::ImageCopyTexture imageCopyTexture = utils::CreateImageCopyTexture(texture, 0, {0, 0, 0});
    wgpu::Extent3D copySize = {kSize, kSize, 1};

    wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
    encoder.CopyTextureToBuffer(&imageCopyTexture, &imageCopyBuffer, &copySize);
    wgpu::CommandBuffer commands = encoder.Finish();
    EXPECT_LAZY_CLEAR(1u, queue.Submit(1, &commands));

    std::vector<uint32_t> expectedWithZeros(bufferSize, 0);
    EXPECT_BUFFER_U32_RANGE_EQ(expectedWithZeros.data(), bufferDst, 0, kSize);

    // Expect texture subresource initialized to be true
    EXPECT_EQ(true, dawn_native::IsTextureSubresourceInitialized(texture.Get(), 0, 1, 0, 1));
}

// This tests that the code path of CopyTextureToBuffer clears correctly for non-renderable textures
TEST_P(TextureZeroInitTest, NonRenderableTextureClearUnalignedSize) {
    // TODO(crbug.com/dawn/667): Work around the fact that some platforms do not support reading
    // from Snorm textures.
    DAWN_TEST_UNSUPPORTED_IF(HasToggleEnabled("disable_snorm_read"));

    wgpu::TextureDescriptor descriptor =
        CreateTextureDescriptor(1, 1, wgpu::TextureUsage::CopySrc, kNonrenderableColorFormat);
    descriptor.size.width = kUnalignedSize;
    descriptor.size.height = kUnalignedSize;
    wgpu::Texture texture = device.CreateTexture(&descriptor);

    // Set buffer with dirty data so we know it is cleared by the lazy cleared texture copy
    uint32_t bytesPerRow =
        Align(kUnalignedSize * kFormatBlockByteSize, kTextureBytesPerRowAlignment);
    uint32_t bufferSize = bytesPerRow * kUnalignedSize;
    std::vector<uint8_t> data(bufferSize, 100);
    wgpu::Buffer bufferDst = utils::CreateBufferFromData(
        device, data.data(), static_cast<uint32_t>(data.size()), wgpu::BufferUsage::CopySrc);
    wgpu::ImageCopyBuffer imageCopyBuffer = utils::CreateImageCopyBuffer(bufferDst, 0, bytesPerRow);
    wgpu::ImageCopyTexture imageCopyTexture = utils::CreateImageCopyTexture(texture, 0, {0, 0, 0});
    wgpu::Extent3D copySize = {kUnalignedSize, kUnalignedSize, 1};

    wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
    encoder.CopyTextureToBuffer(&imageCopyTexture, &imageCopyBuffer, &copySize);
    wgpu::CommandBuffer commands = encoder.Finish();
    EXPECT_LAZY_CLEAR(1u, queue.Submit(1, &commands));

    std::vector<uint32_t> expectedWithZeros(bufferSize, 0);
    EXPECT_BUFFER_U32_RANGE_EQ(expectedWithZeros.data(), bufferDst, 0, kUnalignedSize);

    // Expect texture subresource initialized to be true
    EXPECT_EQ(true, dawn_native::IsTextureSubresourceInitialized(texture.Get(), 0, 1, 0, 1));
}

// This tests that the code path of CopyTextureToBuffer clears correctly for non-renderable textures
// with more than 1 array layers
TEST_P(TextureZeroInitTest, NonRenderableTextureClearWithMultiArrayLayers) {
    // TODO(crbug.com/dawn/667): Work around the fact that some platforms do not support reading
    // from Snorm textures.
    DAWN_TEST_UNSUPPORTED_IF(HasToggleEnabled("disable_snorm_read"));

    wgpu::TextureDescriptor descriptor =
        CreateTextureDescriptor(1, 2, wgpu::TextureUsage::CopySrc, kNonrenderableColorFormat);
    wgpu::Texture texture = device.CreateTexture(&descriptor);

    // Set buffer with dirty data so we know it is cleared by the lazy cleared texture copy
    uint32_t bufferSize = kFormatBlockByteSize * kSize * kSize;
    std::vector<uint8_t> data(bufferSize, 100);
    wgpu::Buffer bufferDst = utils::CreateBufferFromData(
        device, data.data(), static_cast<uint32_t>(data.size()), wgpu::BufferUsage::CopySrc);

    wgpu::ImageCopyBuffer imageCopyBuffer =
        utils::CreateImageCopyBuffer(bufferDst, 0, kSize * kFormatBlockByteSize);
    wgpu::ImageCopyTexture imageCopyTexture = utils::CreateImageCopyTexture(texture, 0, {0, 0, 1});
    wgpu::Extent3D copySize = {kSize, kSize, 1};

    wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
    encoder.CopyTextureToBuffer(&imageCopyTexture, &imageCopyBuffer, &copySize);
    wgpu::CommandBuffer commands = encoder.Finish();
    EXPECT_LAZY_CLEAR(1u, queue.Submit(1, &commands));

    std::vector<uint32_t> expectedWithZeros(bufferSize, 0);
    EXPECT_BUFFER_U32_RANGE_EQ(expectedWithZeros.data(), bufferDst, 0, 8);

    // Expect texture subresource initialized to be true
    EXPECT_EQ(true, dawn_native::IsTextureSubresourceInitialized(texture.Get(), 0, 1, 1, 1));
}

// This tests that storeOp clear resets resource state to uninitialized.
// Start with a sample texture that is initialized with data.
// Then expect the render texture to not store the data from sample texture
// because it will be lazy cleared by the EXPECT_TEXTURE_EQ call.
TEST_P(TextureZeroInitTest, RenderPassStoreOpClear) {
    // Create needed resources
    wgpu::TextureDescriptor descriptor = CreateTextureDescriptor(
        1, 1, wgpu::TextureUsage::TextureBinding | wgpu::TextureUsage::CopyDst, kColorFormat);
    wgpu::Texture texture = device.CreateTexture(&descriptor);

    wgpu::TextureDescriptor renderTextureDescriptor = CreateTextureDescriptor(
        1, 1, wgpu::TextureUsage::CopySrc | wgpu::TextureUsage::RenderAttachment, kColorFormat);
    wgpu::Texture renderTexture = device.CreateTexture(&renderTextureDescriptor);

    // Fill the sample texture with data
    std::vector<uint8_t> data(kFormatBlockByteSize * kSize * kSize, 1);
    wgpu::Buffer stagingBuffer = utils::CreateBufferFromData(
        device, data.data(), static_cast<uint32_t>(data.size()), wgpu::BufferUsage::CopySrc);
    wgpu::ImageCopyBuffer imageCopyBuffer =
        utils::CreateImageCopyBuffer(stagingBuffer, 0, kSize * kFormatBlockByteSize);
    wgpu::ImageCopyTexture imageCopyTexture = utils::CreateImageCopyTexture(texture, 0, {0, 0, 0});
    wgpu::Extent3D copySize = {kSize, kSize, 1};
    wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
    encoder.CopyBufferToTexture(&imageCopyBuffer, &imageCopyTexture, &copySize);
    wgpu::CommandBuffer commands = encoder.Finish();
    // Expect 0 lazy clears because the texture will be completely copied to
    EXPECT_LAZY_CLEAR(0u, queue.Submit(1, &commands));

    // Create render pipeline
    utils::ComboRenderPipelineDescriptor renderPipelineDescriptor;
    renderPipelineDescriptor.vertex.module = CreateBasicVertexShaderForTest();
    renderPipelineDescriptor.cFragment.module = CreateSampledTextureFragmentShaderForTest();
    renderPipelineDescriptor.cTargets[0].format = kColorFormat;
    wgpu::RenderPipeline renderPipeline = device.CreateRenderPipeline(&renderPipelineDescriptor);

    // Create bindgroup
    wgpu::BindGroup bindGroup = utils::MakeBindGroup(device, renderPipeline.GetBindGroupLayout(0),
                                                     {{0, texture.CreateView()}});

    // Encode pass and submit
    encoder = device.CreateCommandEncoder();
    utils::ComboRenderPassDescriptor renderPassDesc({renderTexture.CreateView()});
    renderPassDesc.cColorAttachments[0].clearColor = {0.0, 0.0, 0.0, 0.0};
    renderPassDesc.cColorAttachments[0].loadOp = wgpu::LoadOp::Clear;
    renderPassDesc.cColorAttachments[0].storeOp = wgpu::StoreOp::Discard;
    wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&renderPassDesc);
    pass.SetPipeline(renderPipeline);
    pass.SetBindGroup(0, bindGroup);
    pass.Draw(6);
    pass.EndPass();
    commands = encoder.Finish();
    // Expect 0 lazy clears, sample texture is initialized by copyBufferToTexture and render texture
    // is cleared by loadop
    EXPECT_LAZY_CLEAR(0u, queue.Submit(1, &commands));

    // Expect the rendered texture to be cleared
    std::vector<RGBA8> expectedWithZeros(kSize * kSize, {0, 0, 0, 0});
    EXPECT_LAZY_CLEAR(
        1u, EXPECT_TEXTURE_EQ(expectedWithZeros.data(), renderTexture, {0, 0}, {kSize, kSize}));

    // Expect texture subresource initialized to be true
    EXPECT_EQ(true, dawn_native::IsTextureSubresourceInitialized(texture.Get(), 0, 1, 0, 1));
    EXPECT_EQ(true, dawn_native::IsTextureSubresourceInitialized(renderTexture.Get(), 0, 1, 0, 1));
}

// This tests storeOp Clear on depth and stencil textures.
// We put the depth stencil texture through 2 passes:
// 1) LoadOp::Clear and StoreOp::Discard, fail the depth and stencil test set in the render
//      pipeline. This means nothing is drawn and subresource is set as uninitialized.
// 2) LoadOp::Load and StoreOp::Discard, pass the depth and stencil test set in the render pipeline.
//      Because LoadOp is Load and the subresource is uninitialized, the texture will be cleared to
//      0's This means the depth and stencil test will pass and the red square is drawn.
TEST_P(TextureZeroInitTest, RenderingLoadingDepthStencilStoreOpClear) {
    wgpu::TextureDescriptor srcDescriptor =
        CreateTextureDescriptor(1, 1,
                                wgpu::TextureUsage::CopySrc | wgpu::TextureUsage::CopyDst |
                                    wgpu::TextureUsage::RenderAttachment,
                                kColorFormat);
    wgpu::Texture srcTexture = device.CreateTexture(&srcDescriptor);

    wgpu::TextureDescriptor depthStencilDescriptor =
        CreateTextureDescriptor(1, 1,
                                wgpu::TextureUsage::RenderAttachment | wgpu::TextureUsage::CopySrc |
                                    wgpu::TextureUsage::CopyDst,
                                kDepthStencilFormat);
    wgpu::Texture depthStencilTexture = device.CreateTexture(&depthStencilDescriptor);

    // Setup the renderPass for the first pass.
    // We want to fail the depth and stencil test here so that nothing gets drawn and we can
    // see that the subresource correctly gets set as unintialized in the second pass
    utils::ComboRenderPassDescriptor renderPassDescriptor({srcTexture.CreateView()},
                                                          depthStencilTexture.CreateView());
    renderPassDescriptor.cDepthStencilAttachmentInfo.depthLoadOp = wgpu::LoadOp::Clear;
    renderPassDescriptor.cDepthStencilAttachmentInfo.stencilLoadOp = wgpu::LoadOp::Clear;
    renderPassDescriptor.cDepthStencilAttachmentInfo.clearDepth = 1.0f;
    renderPassDescriptor.cDepthStencilAttachmentInfo.clearStencil = 1u;
    renderPassDescriptor.cDepthStencilAttachmentInfo.depthStoreOp = wgpu::StoreOp::Discard;
    renderPassDescriptor.cDepthStencilAttachmentInfo.stencilStoreOp = wgpu::StoreOp::Discard;
    {
        wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
        wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&renderPassDescriptor);
        pass.SetPipeline(CreatePipelineForTest());
        pass.Draw(6);
        pass.EndPass();
        wgpu::CommandBuffer commandBuffer = encoder.Finish();
        // Expect 0 lazy clears, depth stencil texture will clear using loadop
        EXPECT_LAZY_CLEAR(0u, queue.Submit(1, &commandBuffer));

        // The depth stencil test should fail and not draw because the depth stencil texture is
        // cleared to 1's by using loadOp clear and set values from descriptor.
        std::vector<RGBA8> expectedBlack(kSize * kSize, {0, 0, 0, 0});
        EXPECT_TEXTURE_EQ(expectedBlack.data(), srcTexture, {0, 0}, {kSize, kSize});

        // Expect texture subresource initialized to be false since storeop is clear, sets
        // subresource as uninitialized
        EXPECT_EQ(false, dawn_native::IsTextureSubresourceInitialized(depthStencilTexture.Get(), 0,
                                                                      1, 0, 1));
    }

    // Now we put the depth stencil texture back into renderpass, it should be cleared by loadop
    // because storeOp clear sets the subresource as uninitialized
    {
        renderPassDescriptor.cDepthStencilAttachmentInfo.depthLoadOp = wgpu::LoadOp::Load;
        renderPassDescriptor.cDepthStencilAttachmentInfo.stencilLoadOp = wgpu::LoadOp::Load;
        wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
        wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&renderPassDescriptor);
        pass.SetPipeline(CreatePipelineForTest());
        pass.Draw(6);
        pass.EndPass();
        wgpu::CommandBuffer commandBuffer = encoder.Finish();
        // Expect 0 lazy clears, depth stencil texture will clear using loadop
        EXPECT_LAZY_CLEAR(0u, queue.Submit(1, &commandBuffer));

        // Now the depth stencil test should pass since depth stencil texture is cleared to 0's by
        // loadop load and uninitialized subresource, so we should have a red square
        std::vector<RGBA8> expectedRed(kSize * kSize, {255, 0, 0, 255});
        EXPECT_TEXTURE_EQ(expectedRed.data(), srcTexture, {0, 0}, {kSize, kSize});

        // Expect texture subresource initialized to be false since storeop is clear, sets
        // subresource as uninitialized
        EXPECT_EQ(false, dawn_native::IsTextureSubresourceInitialized(depthStencilTexture.Get(), 0,
                                                                      1, 0, 1));
    }
}

// Test that if one mip of a texture is initialized and another is uninitialized, lazy clearing the
// uninitialized mip does not clear the initialized mip.
TEST_P(TextureZeroInitTest, PreservesInitializedMip) {
    wgpu::TextureDescriptor sampleTextureDescriptor =
        CreateTextureDescriptor(2, 1,
                                wgpu::TextureUsage::CopySrc | wgpu::TextureUsage::CopyDst |
                                    wgpu::TextureUsage::TextureBinding,
                                kColorFormat);
    wgpu::Texture sampleTexture = device.CreateTexture(&sampleTextureDescriptor);

    wgpu::TextureDescriptor renderTextureDescriptor = CreateTextureDescriptor(
        1, 1, wgpu::TextureUsage::CopySrc | wgpu::TextureUsage::RenderAttachment, kColorFormat);
    wgpu::Texture renderTexture = device.CreateTexture(&renderTextureDescriptor);

    // Fill the sample texture's second mip with data
    uint32_t mipSize = kSize >> 1;
    std::vector<uint8_t> data(kFormatBlockByteSize * mipSize * mipSize, 2);
    wgpu::Buffer stagingBuffer = utils::CreateBufferFromData(
        device, data.data(), static_cast<uint32_t>(data.size()), wgpu::BufferUsage::CopySrc);
    wgpu::ImageCopyBuffer imageCopyBuffer =
        utils::CreateImageCopyBuffer(stagingBuffer, 0, mipSize * kFormatBlockByteSize);
    wgpu::ImageCopyTexture imageCopyTexture =
        utils::CreateImageCopyTexture(sampleTexture, 1, {0, 0, 0});
    wgpu::Extent3D copySize = {mipSize, mipSize, 1};
    wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
    encoder.CopyBufferToTexture(&imageCopyBuffer, &imageCopyTexture, &copySize);
    wgpu::CommandBuffer commands = encoder.Finish();
    // Expect 0 lazy clears because the texture subresource will be completely copied to
    EXPECT_LAZY_CLEAR(0u, queue.Submit(1, &commands));

    // Create render pipeline
    utils::ComboRenderPipelineDescriptor renderPipelineDescriptor;
    renderPipelineDescriptor.vertex.module = CreateBasicVertexShaderForTest();
    renderPipelineDescriptor.cFragment.module = CreateSampledTextureFragmentShaderForTest();
    renderPipelineDescriptor.cTargets[0].format = kColorFormat;
    wgpu::RenderPipeline renderPipeline = device.CreateRenderPipeline(&renderPipelineDescriptor);

    // Create bindgroup
    wgpu::BindGroup bindGroup = utils::MakeBindGroup(device, renderPipeline.GetBindGroupLayout(0),
                                                     {{0, sampleTexture.CreateView()}});

    // Encode pass and submit
    encoder = device.CreateCommandEncoder();
    utils::ComboRenderPassDescriptor renderPassDesc({renderTexture.CreateView()});
    renderPassDesc.cColorAttachments[0].clearColor = {0.0, 0.0, 0.0, 0.0};
    renderPassDesc.cColorAttachments[0].loadOp = wgpu::LoadOp::Clear;
    renderPassDesc.cColorAttachments[0].storeOp = wgpu::StoreOp::Discard;
    wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&renderPassDesc);
    pass.SetPipeline(renderPipeline);
    pass.SetBindGroup(0, bindGroup);
    pass.Draw(6);
    pass.EndPass();
    commands = encoder.Finish();
    // Expect 1 lazy clears, because not all mips of the sample texture are initialized by
    // copyBufferToTexture.
    EXPECT_LAZY_CLEAR(1u, queue.Submit(1, &commands));

    // Expect the rendered texture to be cleared since we copied from the uninitialized first
    // mip.
    std::vector<RGBA8> expectedWithZeros(kSize * kSize, {0, 0, 0, 0});
    EXPECT_LAZY_CLEAR(
        1u, EXPECT_TEXTURE_EQ(expectedWithZeros.data(), renderTexture, {0, 0}, {kSize, kSize}, 0));

    // Expect the first mip to have been lazy cleared to 0.
    EXPECT_LAZY_CLEAR(
        0u, EXPECT_TEXTURE_EQ(expectedWithZeros.data(), sampleTexture, {0, 0}, {kSize, kSize}, 0));

    // Expect the second mip to still be filled with 2.
    std::vector<RGBA8> expectedWithTwos(mipSize * mipSize, {2, 2, 2, 2});
    EXPECT_LAZY_CLEAR(0u, EXPECT_TEXTURE_EQ(expectedWithTwos.data(), sampleTexture, {0, 0},
                                            {mipSize, mipSize}, 1));

    // Expect the whole texture to be initialized
    EXPECT_EQ(true, dawn_native::IsTextureSubresourceInitialized(sampleTexture.Get(), 0, 2, 0, 1));
}

// Test that if one layer of a texture is initialized and another is uninitialized, lazy clearing
// the uninitialized layer does not clear the initialized layer.
TEST_P(TextureZeroInitTest, PreservesInitializedArrayLayer) {
    // TODO(crbug.com/dawn/593): This test uses glTextureView() which is not supported on OpenGL ES.
    DAWN_TEST_UNSUPPORTED_IF(IsOpenGLES());

    wgpu::TextureDescriptor sampleTextureDescriptor =
        CreateTextureDescriptor(1, 2,
                                wgpu::TextureUsage::CopySrc | wgpu::TextureUsage::CopyDst |
                                    wgpu::TextureUsage::TextureBinding,
                                kColorFormat);
    wgpu::Texture sampleTexture = device.CreateTexture(&sampleTextureDescriptor);

    wgpu::TextureDescriptor renderTextureDescriptor = CreateTextureDescriptor(
        1, 1, wgpu::TextureUsage::CopySrc | wgpu::TextureUsage::RenderAttachment, kColorFormat);
    wgpu::Texture renderTexture = device.CreateTexture(&renderTextureDescriptor);

    // Fill the sample texture's second array layer with data
    std::vector<uint8_t> data(kFormatBlockByteSize * kSize * kSize, 2);
    wgpu::Buffer stagingBuffer = utils::CreateBufferFromData(
        device, data.data(), static_cast<uint32_t>(data.size()), wgpu::BufferUsage::CopySrc);
    wgpu::ImageCopyBuffer imageCopyBuffer =
        utils::CreateImageCopyBuffer(stagingBuffer, 0, kSize * kFormatBlockByteSize);
    wgpu::ImageCopyTexture imageCopyTexture =
        utils::CreateImageCopyTexture(sampleTexture, 0, {0, 0, 1});
    wgpu::Extent3D copySize = {kSize, kSize, 1};
    wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
    encoder.CopyBufferToTexture(&imageCopyBuffer, &imageCopyTexture, &copySize);
    wgpu::CommandBuffer commands = encoder.Finish();
    // Expect 0 lazy clears because the texture subresource will be completely copied to
    EXPECT_LAZY_CLEAR(0u, queue.Submit(1, &commands));

    // Create render pipeline
    utils::ComboRenderPipelineDescriptor renderPipelineDescriptor;
    renderPipelineDescriptor.vertex.module = CreateBasicVertexShaderForTest();
    renderPipelineDescriptor.cFragment.module = CreateSampledTextureFragmentShaderForTest();
    renderPipelineDescriptor.cTargets[0].format = kColorFormat;
    wgpu::RenderPipeline renderPipeline = device.CreateRenderPipeline(&renderPipelineDescriptor);

    // Only sample from the uninitialized first layer.
    wgpu::TextureViewDescriptor textureViewDescriptor;
    textureViewDescriptor.dimension = wgpu::TextureViewDimension::e2D;
    textureViewDescriptor.arrayLayerCount = 1;

    // Create bindgroup
    wgpu::BindGroup bindGroup =
        utils::MakeBindGroup(device, renderPipeline.GetBindGroupLayout(0),
                             {{0, sampleTexture.CreateView(&textureViewDescriptor)}});

    // Encode pass and submit
    encoder = device.CreateCommandEncoder();
    utils::ComboRenderPassDescriptor renderPassDesc({renderTexture.CreateView()});
    renderPassDesc.cColorAttachments[0].clearColor = {0.0, 0.0, 0.0, 0.0};
    renderPassDesc.cColorAttachments[0].loadOp = wgpu::LoadOp::Clear;
    renderPassDesc.cColorAttachments[0].storeOp = wgpu::StoreOp::Discard;
    wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&renderPassDesc);
    pass.SetPipeline(renderPipeline);
    pass.SetBindGroup(0, bindGroup);
    pass.Draw(6);
    pass.EndPass();
    commands = encoder.Finish();
    // Expect 1 lazy clears, because not all array layers of the sample texture are initialized by
    // copyBufferToTexture.
    EXPECT_LAZY_CLEAR(1u, queue.Submit(1, &commands));

    // Expect the rendered texture to be cleared since we copied from the uninitialized first
    // array layer.
    std::vector<RGBA8> expectedWithZeros(kSize * kSize, {0, 0, 0, 0});
    EXPECT_LAZY_CLEAR(
        1u, EXPECT_TEXTURE_EQ(expectedWithZeros.data(), renderTexture, {0, 0, 0}, {kSize, kSize}));

    // Expect the first array layer to have been lazy cleared to 0.
    EXPECT_LAZY_CLEAR(
        0u, EXPECT_TEXTURE_EQ(expectedWithZeros.data(), sampleTexture, {0, 0, 0}, {kSize, kSize}));

    // Expect the second array layer to still be filled with 2.
    std::vector<RGBA8> expectedWithTwos(kSize * kSize, {2, 2, 2, 2});
    EXPECT_LAZY_CLEAR(
        0u, EXPECT_TEXTURE_EQ(expectedWithTwos.data(), sampleTexture, {0, 0, 1}, {kSize, kSize}));

    // Expect the whole texture to be initialized
    EXPECT_EQ(true, dawn_native::IsTextureSubresourceInitialized(sampleTexture.Get(), 0, 1, 0, 2));
}

// This is a regression test for crbug.com/dawn/451 where the lazy texture
// init path on D3D12 had a divide-by-zero exception in the copy split logic.
TEST_P(TextureZeroInitTest, CopyTextureToBufferNonRenderableUnaligned) {
    // TODO(crbug.com/dawn/667): Work around the fact that some platforms do not support reading
    // from Snorm textures.
    DAWN_TEST_UNSUPPORTED_IF(HasToggleEnabled("disable_snorm_read"));

    wgpu::TextureDescriptor descriptor;
    descriptor.size.width = kUnalignedSize;
    descriptor.size.height = kUnalignedSize;
    descriptor.size.depthOrArrayLayers = 1;
    descriptor.format = wgpu::TextureFormat::R8Snorm;
    descriptor.usage = wgpu::TextureUsage::CopySrc;
    wgpu::Texture texture = device.CreateTexture(&descriptor);

    {
        uint32_t bytesPerRow = Align(kUnalignedSize, kTextureBytesPerRowAlignment);

        // Create and initialize the destination buffer to ensure we only count the times of
        // texture lazy initialization in this test.
        const uint64_t bufferSize = kUnalignedSize * bytesPerRow;
        const std::vector<uint8_t> initialBufferData(bufferSize, 0u);
        wgpu::Buffer buffer = utils::CreateBufferFromData(device, initialBufferData.data(),
                                                          bufferSize, wgpu::BufferUsage::CopyDst);

        wgpu::ImageCopyTexture imageCopyTexture =
            utils::CreateImageCopyTexture(texture, 0, {0, 0, 0});
        wgpu::ImageCopyBuffer imageCopyBuffer =
            utils::CreateImageCopyBuffer(buffer, 0, bytesPerRow);
        wgpu::Extent3D copySize = {kUnalignedSize, kUnalignedSize, 1};

        wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
        encoder.CopyTextureToBuffer(&imageCopyTexture, &imageCopyBuffer, &copySize);

        wgpu::CommandBuffer commands = encoder.Finish();
        EXPECT_LAZY_CLEAR(1u, queue.Submit(1, &commands));
    }

    // Expect texture subresource initialized to be true
    EXPECT_EQ(true, dawn_native::IsTextureSubresourceInitialized(texture.Get(), 0, 1, 0, 1));
}

// In this test WriteTexture fully overwrites a texture
TEST_P(TextureZeroInitTest, WriteWholeTexture) {
    wgpu::TextureDescriptor descriptor = CreateTextureDescriptor(
        1, 1, wgpu::TextureUsage::CopyDst | wgpu::TextureUsage::CopySrc, kColorFormat);
    wgpu::Texture texture = device.CreateTexture(&descriptor);

    wgpu::ImageCopyTexture imageCopyTexture = utils::CreateImageCopyTexture(texture, 0, {0, 0, 0});
    wgpu::Extent3D copySize = {kSize, kSize, 1};

    wgpu::TextureDataLayout textureDataLayout;
    textureDataLayout.offset = 0;
    textureDataLayout.bytesPerRow = kSize * kFormatBlockByteSize;
    textureDataLayout.rowsPerImage = kSize;

    std::vector<RGBA8> data(
        utils::RequiredBytesInCopy(textureDataLayout.bytesPerRow, textureDataLayout.rowsPerImage,
                                   copySize, kColorFormat) /
            sizeof(RGBA8),
        {100, 100, 100, 100});

    // The write overwrites the whole texture so we don't need to do lazy initialization.
    EXPECT_LAZY_CLEAR(
        0u, queue.WriteTexture(&imageCopyTexture, data.data(), data.size() * sizeof(RGBA8),
                               &textureDataLayout, &copySize));

    // Expect texture initialized to be true
    EXPECT_TRUE(dawn_native::IsTextureSubresourceInitialized(texture.Get(), 0, 1, 0, 1));

    EXPECT_TEXTURE_EQ(data.data(), texture, {0, 0}, {kSize, kSize});
}

// Test WriteTexture to a subset of the texture, lazy init is necessary to clear the other
// half.
TEST_P(TextureZeroInitTest, WriteTextureHalf) {
    wgpu::TextureDescriptor descriptor =
        CreateTextureDescriptor(4, 1,
                                wgpu::TextureUsage::CopyDst | wgpu::TextureUsage::TextureBinding |
                                    wgpu::TextureUsage::CopySrc,
                                kColorFormat);
    wgpu::Texture texture = device.CreateTexture(&descriptor);

    wgpu::ImageCopyTexture imageCopyTexture = utils::CreateImageCopyTexture(texture, 0, {0, 0, 0});
    wgpu::Extent3D copySize = {kSize / 2, kSize, 1};

    wgpu::TextureDataLayout textureDataLayout;
    textureDataLayout.offset = 0;
    textureDataLayout.bytesPerRow = kSize * kFormatBlockByteSize / 2;
    textureDataLayout.rowsPerImage = kSize;

    std::vector<RGBA8> data(
        utils::RequiredBytesInCopy(textureDataLayout.bytesPerRow, textureDataLayout.rowsPerImage,
                                   copySize, kColorFormat) /
            sizeof(RGBA8),
        {100, 100, 100, 100});

    EXPECT_LAZY_CLEAR(
        1u, queue.WriteTexture(&imageCopyTexture, data.data(), data.size() * sizeof(RGBA8),
                               &textureDataLayout, &copySize));

    // Expect texture initialized to be true
    EXPECT_EQ(true, dawn_native::IsTextureSubresourceInitialized(texture.Get(), 0, 1, 0, 1));

    std::vector<RGBA8> expectedZeros((kSize / 2) * kSize, {0, 0, 0, 0});
    // first half filled with 100, by the data
    EXPECT_TEXTURE_EQ(data.data(), texture, {0, 0}, {kSize / 2, kSize});
    // second half should be cleared
    EXPECT_TEXTURE_EQ(expectedZeros.data(), texture, {kSize / 2, 0}, {kSize / 2, kSize});
}

// In this test WriteTexture fully overwrites a range of subresources, so lazy initialization
// is needed for neither the subresources involved in the write nor the other subresources.
TEST_P(TextureZeroInitTest, WriteWholeTextureArray) {
    wgpu::TextureDescriptor descriptor = CreateTextureDescriptor(
        1, 6, wgpu::TextureUsage::CopyDst | wgpu::TextureUsage::CopySrc, kColorFormat);
    wgpu::Texture texture = device.CreateTexture(&descriptor);

    constexpr uint32_t kBaseArrayLayer = 2u;
    constexpr uint32_t kCopyLayerCount = 3u;

    wgpu::ImageCopyTexture imageCopyTexture =
        utils::CreateImageCopyTexture(texture, 0, {0, 0, kBaseArrayLayer});
    wgpu::Extent3D copySize = {kSize, kSize, kCopyLayerCount};

    wgpu::TextureDataLayout textureDataLayout;
    textureDataLayout.offset = 0;
    textureDataLayout.bytesPerRow = kSize * kFormatBlockByteSize;
    textureDataLayout.rowsPerImage = kSize;

    std::vector<RGBA8> data(
        utils::RequiredBytesInCopy(textureDataLayout.bytesPerRow, textureDataLayout.rowsPerImage,
                                   copySize, kColorFormat) /
            sizeof(RGBA8),
        {100, 100, 100, 100});

    // The write overwrites the whole subresources so we don't need to do lazy initialization on
    // them.
    EXPECT_LAZY_CLEAR(
        0u, queue.WriteTexture(&imageCopyTexture, data.data(), data.size() * sizeof(RGBA8),
                               &textureDataLayout, &copySize));

    // Expect texture subresource initialized to be true
    EXPECT_TRUE(dawn_native::IsTextureSubresourceInitialized(texture.Get(), 0, 1, kBaseArrayLayer,
                                                             kCopyLayerCount));

    for (uint32_t layer = kBaseArrayLayer; layer < kBaseArrayLayer + kCopyLayerCount; ++layer) {
        EXPECT_TEXTURE_EQ(data.data(), texture, {0, 0, layer}, {kSize, kSize});
    }
}

// Test WriteTexture to a subset of the subresource, lazy init is necessary to clear the other
// half.
TEST_P(TextureZeroInitTest, WriteTextureArrayHalf) {
    wgpu::TextureDescriptor descriptor =
        CreateTextureDescriptor(4, 6,
                                wgpu::TextureUsage::CopyDst | wgpu::TextureUsage::TextureBinding |
                                    wgpu::TextureUsage::CopySrc,
                                kColorFormat);
    wgpu::Texture texture = device.CreateTexture(&descriptor);

    constexpr uint32_t kBaseArrayLayer = 2u;
    constexpr uint32_t kCopyLayerCount = 3u;

    wgpu::ImageCopyTexture imageCopyTexture =
        utils::CreateImageCopyTexture(texture, 0, {0, 0, kBaseArrayLayer});
    wgpu::Extent3D copySize = {kSize / 2, kSize, kCopyLayerCount};

    wgpu::TextureDataLayout textureDataLayout;
    textureDataLayout.offset = 0;
    textureDataLayout.bytesPerRow = kSize * kFormatBlockByteSize / 2;
    textureDataLayout.rowsPerImage = kSize;

    std::vector<RGBA8> data(
        utils::RequiredBytesInCopy(textureDataLayout.bytesPerRow, textureDataLayout.rowsPerImage,
                                   copySize, kColorFormat) /
            sizeof(RGBA8),
        {100, 100, 100, 100});

    EXPECT_LAZY_CLEAR(
        1u, queue.WriteTexture(&imageCopyTexture, data.data(), data.size() * sizeof(RGBA8),
                               &textureDataLayout, &copySize));

    // Expect texture subresource initialized to be true
    EXPECT_EQ(true, dawn_native::IsTextureSubresourceInitialized(texture.Get(), 0, 1,
                                                                 kBaseArrayLayer, kCopyLayerCount));

    std::vector<RGBA8> expectedZeros((kSize / 2) * kSize, {0, 0, 0, 0});
    for (uint32_t layer = kBaseArrayLayer; layer < kBaseArrayLayer + kCopyLayerCount; ++layer) {
        // first half filled with 100, by the data
        EXPECT_TEXTURE_EQ(data.data(), texture, {0, 0, layer}, {kSize / 2, kSize});
        // second half should be cleared
        EXPECT_TEXTURE_EQ(expectedZeros.data(), texture, {kSize / 2, 0, layer}, {kSize / 2, kSize});
    }
}

// In this test WriteTexture fully overwrites a texture at mip level.
TEST_P(TextureZeroInitTest, WriteWholeTextureAtMipLevel) {
    wgpu::TextureDescriptor descriptor = CreateTextureDescriptor(
        4, 1, wgpu::TextureUsage::CopyDst | wgpu::TextureUsage::CopySrc, kColorFormat);
    wgpu::Texture texture = device.CreateTexture(&descriptor);

    constexpr uint32_t kMipLevel = 2;
    constexpr uint32_t kMipSize = kSize >> kMipLevel;

    wgpu::ImageCopyTexture imageCopyTexture =
        utils::CreateImageCopyTexture(texture, kMipLevel, {0, 0, 0});
    wgpu::Extent3D copySize = {kMipSize, kMipSize, 1};

    wgpu::TextureDataLayout textureDataLayout;
    textureDataLayout.offset = 0;
    textureDataLayout.bytesPerRow = kMipSize * kFormatBlockByteSize;
    textureDataLayout.rowsPerImage = kMipSize;

    std::vector<RGBA8> data(
        utils::RequiredBytesInCopy(textureDataLayout.bytesPerRow, textureDataLayout.rowsPerImage,
                                   copySize, kColorFormat) /
            sizeof(RGBA8),
        {100, 100, 100, 100});

    // The write overwrites the whole texture so we don't need to do lazy initialization.
    EXPECT_LAZY_CLEAR(
        0u, queue.WriteTexture(&imageCopyTexture, data.data(), data.size() * sizeof(RGBA8),
                               &textureDataLayout, &copySize));

    // Expect texture initialized to be true
    EXPECT_TRUE(dawn_native::IsTextureSubresourceInitialized(texture.Get(), kMipLevel, 1, 0, 1));

    EXPECT_TEXTURE_EQ(data.data(), texture, {0, 0}, {kMipSize, kMipSize}, kMipLevel);
}

// Test WriteTexture to a subset of the texture at mip level, lazy init is necessary to clear the
// other half.
TEST_P(TextureZeroInitTest, WriteTextureHalfAtMipLevel) {
    wgpu::TextureDescriptor descriptor =
        CreateTextureDescriptor(4, 1,
                                wgpu::TextureUsage::CopyDst | wgpu::TextureUsage::TextureBinding |
                                    wgpu::TextureUsage::CopySrc,
                                kColorFormat);
    wgpu::Texture texture = device.CreateTexture(&descriptor);

    constexpr uint32_t kMipLevel = 2;
    constexpr uint32_t kMipSize = kSize >> kMipLevel;

    wgpu::ImageCopyTexture imageCopyTexture =
        utils::CreateImageCopyTexture(texture, kMipLevel, {0, 0, 0});
    wgpu::Extent3D copySize = {kMipSize / 2, kMipSize, 1};

    wgpu::TextureDataLayout textureDataLayout;
    textureDataLayout.offset = 0;
    textureDataLayout.bytesPerRow = kMipSize * kFormatBlockByteSize / 2;
    textureDataLayout.rowsPerImage = kMipSize;

    std::vector<RGBA8> data(
        utils::RequiredBytesInCopy(textureDataLayout.bytesPerRow, textureDataLayout.rowsPerImage,
                                   copySize, kColorFormat) /
            sizeof(RGBA8),
        {100, 100, 100, 100});

    EXPECT_LAZY_CLEAR(
        1u, queue.WriteTexture(&imageCopyTexture, data.data(), data.size() * sizeof(RGBA8),
                               &textureDataLayout, &copySize));

    // Expect texture initialized to be true
    EXPECT_EQ(true,
              dawn_native::IsTextureSubresourceInitialized(texture.Get(), kMipLevel, 1, 0, 1));

    std::vector<RGBA8> expectedZeros((kMipSize / 2) * kMipSize, {0, 0, 0, 0});
    // first half filled with 100, by the data
    EXPECT_TEXTURE_EQ(data.data(), texture, {0, 0}, {kMipSize / 2, kMipSize}, kMipLevel);
    // second half should be cleared
    EXPECT_TEXTURE_EQ(expectedZeros.data(), texture, {kMipSize / 2, 0}, {kMipSize / 2, kMipSize},
                      kMipLevel);
}

DAWN_INSTANTIATE_TEST(TextureZeroInitTest,
                      D3D12Backend({"nonzero_clear_resources_on_creation_for_testing"}),
                      D3D12Backend({"nonzero_clear_resources_on_creation_for_testing"},
                                   {"use_d3d12_render_pass"}),
                      OpenGLBackend({"nonzero_clear_resources_on_creation_for_testing"}),
                      OpenGLESBackend({"nonzero_clear_resources_on_creation_for_testing"}),
                      MetalBackend({"nonzero_clear_resources_on_creation_for_testing"}),
                      VulkanBackend({"nonzero_clear_resources_on_creation_for_testing"}));

class CompressedTextureZeroInitTest : public TextureZeroInitTest {
  protected:
    void SetUp() override {
        DawnTest::SetUp();

        DAWN_TEST_UNSUPPORTED_IF(UsesWire());
        DAWN_TEST_UNSUPPORTED_IF(!IsBCFormatSupported());
    }

    std::vector<const char*> GetRequiredFeatures() override {
        mIsBCFormatSupported = SupportsFeatures({"texture-compression-bc"});
        if (!mIsBCFormatSupported) {
            return {};
        }

        return {"texture-compression-bc"};
    }

    bool IsBCFormatSupported() const {
        return mIsBCFormatSupported;
    }

    // Copy the compressed texture data into the destination texture.
    void InitializeDataInCompressedTextureAndExpectLazyClear(
        wgpu::Texture bcCompressedTexture,
        wgpu::TextureDescriptor textureDescriptor,
        wgpu::Extent3D copyExtent3D,
        uint32_t viewMipmapLevel,
        uint32_t baseArrayLayer,
        size_t lazyClearCount) {
        uint32_t copyWidthInBlock = copyExtent3D.width / kFormatBlockByteSize;
        uint32_t copyHeightInBlock = copyExtent3D.height / kFormatBlockByteSize;
        uint32_t copyBytesPerRow =
            Align(copyWidthInBlock * utils::GetTexelBlockSizeInBytes(textureDescriptor.format),
                  kTextureBytesPerRowAlignment);

        std::vector<uint8_t> data(
            utils::RequiredBytesInCopy(copyBytesPerRow, copyHeightInBlock, copyExtent3D,
                                       textureDescriptor.format),
            1);

        // Copy texture data from a staging buffer to the destination texture.
        wgpu::Buffer stagingBuffer = utils::CreateBufferFromData(device, data.data(), data.size(),
                                                                 wgpu::BufferUsage::CopySrc);
        wgpu::ImageCopyBuffer imageCopyBuffer =
            utils::CreateImageCopyBuffer(stagingBuffer, 0, copyBytesPerRow, copyHeightInBlock);

        wgpu::ImageCopyTexture imageCopyTexture = utils::CreateImageCopyTexture(
            bcCompressedTexture, viewMipmapLevel, {0, 0, baseArrayLayer});

        wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
        encoder.CopyBufferToTexture(&imageCopyBuffer, &imageCopyTexture, &copyExtent3D);
        wgpu::CommandBuffer copy = encoder.Finish();
        EXPECT_LAZY_CLEAR(lazyClearCount, queue.Submit(1, &copy));
    }

    // Run the tests that copies pre-prepared BC format data into a BC texture and verifies if we
    // can render correctly with the pixel values sampled from the BC texture.
    // Expect that the texture subresource is initialized
    void TestCopyRegionIntoBCFormatTexturesAndCheckSubresourceIsInitialized(
        wgpu::TextureDescriptor textureDescriptor,
        wgpu::Extent3D copyExtent3D,
        wgpu::Extent3D nonPaddedCopyExtent,
        uint32_t viewMipmapLevel,
        uint32_t baseArrayLayer,
        size_t lazyClearCount,
        bool halfCopyTest = false) {
        wgpu::Texture bcTexture = device.CreateTexture(&textureDescriptor);
        InitializeDataInCompressedTextureAndExpectLazyClear(bcTexture, textureDescriptor,
                                                            copyExtent3D, viewMipmapLevel,
                                                            baseArrayLayer, lazyClearCount);

        SampleCompressedTextureAndVerifyColor(bcTexture, textureDescriptor, copyExtent3D,
                                              nonPaddedCopyExtent, viewMipmapLevel, baseArrayLayer,
                                              halfCopyTest);
    }

    void SampleCompressedTextureAndVerifyColor(wgpu::Texture bcTexture,
                                               wgpu::TextureDescriptor textureDescriptor,
                                               wgpu::Extent3D copyExtent3D,
                                               wgpu::Extent3D nonPaddedCopyExtent,
                                               uint32_t viewMipmapLevel,
                                               uint32_t baseArrayLayer,
                                               bool halfCopyTest = false) {
        // Sample the compressed texture and verify the texture colors in the render target
        utils::BasicRenderPass renderPass =
            utils::CreateBasicRenderPass(device, textureDescriptor.size.width >> viewMipmapLevel,
                                         textureDescriptor.size.height >> viewMipmapLevel);

        wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
        {
            wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&renderPass.renderPassInfo);
            utils::ComboRenderPipelineDescriptor renderPipelineDescriptor;
            renderPipelineDescriptor.cTargets[0].format = kColorFormat;
            renderPipelineDescriptor.vertex.module = CreateBasicVertexShaderForTest();
            renderPipelineDescriptor.cFragment.module = CreateSampledTextureFragmentShaderForTest();
            wgpu::RenderPipeline renderPipeline =
                device.CreateRenderPipeline(&renderPipelineDescriptor);
            pass.SetPipeline(renderPipeline);

            wgpu::TextureViewDescriptor textureViewDescriptor = CreateTextureViewDescriptor(
                viewMipmapLevel, baseArrayLayer, textureDescriptor.format);
            wgpu::BindGroup bindGroup =
                utils::MakeBindGroup(device, renderPipeline.GetBindGroupLayout(0),
                                     {{0, bcTexture.CreateView(&textureViewDescriptor)}});
            pass.SetBindGroup(0, bindGroup);
            pass.Draw(6);
            pass.EndPass();
        }

        wgpu::CommandBuffer commands = encoder.Finish();
        queue.Submit(1, &commands);

        std::vector<RGBA8> expected(nonPaddedCopyExtent.width * nonPaddedCopyExtent.height,
                                    {0x00, 0x20, 0x08, 0xFF});
        EXPECT_TEXTURE_EQ(expected.data(), renderPass.color, {0, 0},
                          {nonPaddedCopyExtent.width, nonPaddedCopyExtent.height});
        EXPECT_TRUE(dawn_native::IsTextureSubresourceInitialized(bcTexture.Get(), viewMipmapLevel,
                                                                 1, baseArrayLayer, 1));

        // If we only copied to half the texture, check the other half is initialized to black
        if (halfCopyTest) {
            std::vector<RGBA8> expectBlack(nonPaddedCopyExtent.width * nonPaddedCopyExtent.height,
                                           {0x00, 0x00, 0x00, 0xFF});
            EXPECT_TEXTURE_EQ(expectBlack.data(), renderPass.color, {copyExtent3D.width, 0},
                              {nonPaddedCopyExtent.width, nonPaddedCopyExtent.height});
        }
    }

    bool mIsBCFormatSupported = false;
};

//  Test that the clearing is skipped when we use a full mip copy (with the physical size different
//  than the virtual mip size)
TEST_P(CompressedTextureZeroInitTest, FullMipCopy) {
    wgpu::TextureDescriptor textureDescriptor;
    textureDescriptor.usage = wgpu::TextureUsage::CopySrc | wgpu::TextureUsage::CopyDst |
                              wgpu::TextureUsage::TextureBinding;
    textureDescriptor.size = {60, 60, 1};
    textureDescriptor.mipLevelCount = 1;
    textureDescriptor.format = utils::kBCFormats[0];

    TestCopyRegionIntoBCFormatTexturesAndCheckSubresourceIsInitialized(
        textureDescriptor, textureDescriptor.size, textureDescriptor.size, 0, 0, 0u);
}

// Test that 1 lazy clear count happens when we copy to half the texture
TEST_P(CompressedTextureZeroInitTest, HalfCopyBufferToTexture) {
    // TODO(crbug.com/dawn/643): diagnose and fix this failure on OpenGL.
    DAWN_SUPPRESS_TEST_IF(IsOpenGL() || IsOpenGLES());

    wgpu::TextureDescriptor textureDescriptor;
    textureDescriptor.usage = wgpu::TextureUsage::CopySrc | wgpu::TextureUsage::CopyDst |
                              wgpu::TextureUsage::TextureBinding;
    constexpr static uint32_t kSize = 16;
    textureDescriptor.size = {kSize, kSize, 1};
    textureDescriptor.mipLevelCount = 1;
    textureDescriptor.format = utils::kBCFormats[0];

    wgpu::Extent3D copyExtent3D = {kSize / 2, kSize, 1};

    TestCopyRegionIntoBCFormatTexturesAndCheckSubresourceIsInitialized(
        textureDescriptor, copyExtent3D, copyExtent3D, 0, 0, 1u, true);
}

// Test that 0 lazy clear count happens when we copy buffer to texture to a nonzero mip level
// (with physical size different from the virtual mip size)
TEST_P(CompressedTextureZeroInitTest, FullCopyToNonZeroMipLevel) {
    // TODO(crbug.com/dawn/593): This test uses glTextureView() which is not supported on OpenGL ES.
    DAWN_TEST_UNSUPPORTED_IF(IsOpenGLES());

    wgpu::TextureDescriptor textureDescriptor;
    textureDescriptor.usage = wgpu::TextureUsage::CopySrc | wgpu::TextureUsage::CopyDst |
                              wgpu::TextureUsage::TextureBinding;
    constexpr static uint32_t kSize = 60;
    textureDescriptor.size = {kSize, kSize, 1};
    textureDescriptor.mipLevelCount = 3;
    textureDescriptor.format = utils::kBCFormats[0];
    const uint32_t kViewMipLevel = 2;
    const uint32_t kActualSizeAtLevel = kSize >> kViewMipLevel;

    const uint32_t kCopySizeAtLevel = Align(kActualSizeAtLevel, kFormatBlockByteSize);

    wgpu::Extent3D copyExtent3D = {kCopySizeAtLevel, kCopySizeAtLevel, 1};

    TestCopyRegionIntoBCFormatTexturesAndCheckSubresourceIsInitialized(
        textureDescriptor, copyExtent3D, {kActualSizeAtLevel, kActualSizeAtLevel, 1}, kViewMipLevel,
        0, 0u);
}

// Test that 1 lazy clear count happens when we copy buffer to half texture to a nonzero mip level
// (with physical size different from the virtual mip size)
TEST_P(CompressedTextureZeroInitTest, HalfCopyToNonZeroMipLevel) {
    // TODO(crbug.com/dawn/643): diagnose and fix this failure on OpenGL.
    DAWN_SUPPRESS_TEST_IF(IsOpenGL() || IsOpenGLES());

    wgpu::TextureDescriptor textureDescriptor;
    textureDescriptor.usage = wgpu::TextureUsage::CopySrc | wgpu::TextureUsage::CopyDst |
                              wgpu::TextureUsage::TextureBinding;
    constexpr static uint32_t kSize = 60;
    textureDescriptor.size = {kSize, kSize, 1};
    textureDescriptor.mipLevelCount = 3;
    textureDescriptor.format = utils::kBCFormats[0];
    const uint32_t kViewMipLevel = 2;
    const uint32_t kActualSizeAtLevel = kSize >> kViewMipLevel;

    const uint32_t kCopySizeAtLevel = Align(kActualSizeAtLevel, kFormatBlockByteSize);

    wgpu::Extent3D copyExtent3D = {kCopySizeAtLevel / 2, kCopySizeAtLevel, 1};

    TestCopyRegionIntoBCFormatTexturesAndCheckSubresourceIsInitialized(
        textureDescriptor, copyExtent3D, {kActualSizeAtLevel / 2, kActualSizeAtLevel, 1},
        kViewMipLevel, 0, 1u, true);
}

// Test that 0 lazy clear count happens when we copy buffer to nonzero array layer
TEST_P(CompressedTextureZeroInitTest, FullCopyToNonZeroArrayLayer) {
    // TODO(crbug.com/dawn/593): This test uses glTextureView() which is not supported on OpenGL ES.
    DAWN_TEST_UNSUPPORTED_IF(IsOpenGLES());

    wgpu::TextureDescriptor textureDescriptor;
    textureDescriptor.usage = wgpu::TextureUsage::CopySrc | wgpu::TextureUsage::CopyDst |
                              wgpu::TextureUsage::TextureBinding;
    constexpr static uint32_t kSize = 16;
    constexpr static uint32_t kArrayLayers = 4;
    textureDescriptor.size = {kSize, kSize, kArrayLayers};
    textureDescriptor.mipLevelCount = 1;
    textureDescriptor.format = utils::kBCFormats[0];

    wgpu::Extent3D copyExtent3D = {kSize, kSize, 1};

    TestCopyRegionIntoBCFormatTexturesAndCheckSubresourceIsInitialized(
        textureDescriptor, copyExtent3D, copyExtent3D, 0, kArrayLayers - 2, 0u);
}

// Test that 1 lazy clear count happens when we copy buffer to half texture to a nonzero array layer
TEST_P(CompressedTextureZeroInitTest, HalfCopyToNonZeroArrayLayer) {
    // TODO(crbug.com/dawn/643): diagnose and fix this failure on OpenGL.
    DAWN_SUPPRESS_TEST_IF(IsOpenGL() || IsOpenGLES());

    wgpu::TextureDescriptor textureDescriptor;
    textureDescriptor.usage = wgpu::TextureUsage::CopySrc | wgpu::TextureUsage::CopyDst |
                              wgpu::TextureUsage::TextureBinding;
    constexpr static uint32_t kSize = 16;
    constexpr static uint32_t kArrayLayers = 4;
    textureDescriptor.size = {kSize, kSize, kArrayLayers};
    textureDescriptor.mipLevelCount = 3;
    textureDescriptor.format = utils::kBCFormats[0];

    wgpu::Extent3D copyExtent3D = {kSize / 2, kSize, 1};

    TestCopyRegionIntoBCFormatTexturesAndCheckSubresourceIsInitialized(
        textureDescriptor, copyExtent3D, copyExtent3D, 0, kArrayLayers - 2, 1u, true);
}

// full copy texture to texture, 0 lazy clears are needed
TEST_P(CompressedTextureZeroInitTest, FullCopyTextureToTextureMipLevel) {
    // TODO(crbug.com/dawn/593): This test uses glTextureView() which is not supported on OpenGL ES.
    DAWN_TEST_UNSUPPORTED_IF(IsOpenGLES());

    // create srcTexture and fill it with data
    wgpu::TextureDescriptor srcDescriptor =
        CreateTextureDescriptor(3, 1,
                                wgpu::TextureUsage::TextureBinding | wgpu::TextureUsage::CopySrc |
                                    wgpu::TextureUsage::CopyDst,
                                utils::kBCFormats[0]);
    wgpu::Texture srcTexture = device.CreateTexture(&srcDescriptor);

    const uint32_t kViewMipLevel = 2;
    const uint32_t kActualSizeAtLevel = kSize >> kViewMipLevel;

    const uint32_t kCopySizeAtLevel = Align(kActualSizeAtLevel, kFormatBlockByteSize);

    wgpu::Extent3D copyExtent3D = {kCopySizeAtLevel, kCopySizeAtLevel, 1};

    // fill srcTexture with data
    InitializeDataInCompressedTextureAndExpectLazyClear(srcTexture, srcDescriptor, copyExtent3D,
                                                        kViewMipLevel, 0, 0u);

    wgpu::ImageCopyTexture srcImageCopyTexture =
        utils::CreateImageCopyTexture(srcTexture, kViewMipLevel, {0, 0, 0});

    // create dstTexture that we will copy to
    wgpu::TextureDescriptor dstDescriptor =
        CreateTextureDescriptor(3, 1,
                                wgpu::TextureUsage::CopyDst | wgpu::TextureUsage::CopySrc |
                                    wgpu::TextureUsage::TextureBinding,
                                utils::kBCFormats[0]);
    wgpu::Texture dstTexture = device.CreateTexture(&dstDescriptor);

    wgpu::ImageCopyTexture dstImageCopyTexture =
        utils::CreateImageCopyTexture(dstTexture, kViewMipLevel, {0, 0, 0});

    wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
    encoder.CopyTextureToTexture(&srcImageCopyTexture, &dstImageCopyTexture, &copyExtent3D);
    wgpu::CommandBuffer commands = encoder.Finish();
    // the dstTexture does not need to be lazy cleared since it's fully copied to
    EXPECT_LAZY_CLEAR(0u, queue.Submit(1, &commands));

    SampleCompressedTextureAndVerifyColor(dstTexture, dstDescriptor, copyExtent3D,
                                          {kActualSizeAtLevel, kActualSizeAtLevel, 1},
                                          kViewMipLevel, 0);
}

// half copy texture to texture, lazy clears are needed for noncopied half
TEST_P(CompressedTextureZeroInitTest, HalfCopyTextureToTextureMipLevel) {
    // TODO(crbug.com/dawn/643): diagnose and fix this failure on OpenGL.
    DAWN_SUPPRESS_TEST_IF(IsOpenGL() || IsOpenGLES());

    // create srcTexture with data
    wgpu::TextureDescriptor srcDescriptor =
        CreateTextureDescriptor(3, 1,
                                wgpu::TextureUsage::TextureBinding | wgpu::TextureUsage::CopySrc |
                                    wgpu::TextureUsage::CopyDst,
                                utils::kBCFormats[0]);
    wgpu::Texture srcTexture = device.CreateTexture(&srcDescriptor);

    const uint32_t kViewMipLevel = 2;
    const uint32_t kActualSizeAtLevel = kSize >> kViewMipLevel;

    const uint32_t kCopySizeAtLevel = Align(kActualSizeAtLevel, kFormatBlockByteSize);

    wgpu::Extent3D copyExtent3D = {kCopySizeAtLevel / 2, kCopySizeAtLevel, 1};

    // fill srcTexture with data
    InitializeDataInCompressedTextureAndExpectLazyClear(srcTexture, srcDescriptor, copyExtent3D,
                                                        kViewMipLevel, 0, 1u);

    wgpu::ImageCopyTexture srcImageCopyTexture =
        utils::CreateImageCopyTexture(srcTexture, kViewMipLevel, {0, 0, 0});

    // create dstTexture that we will copy to
    wgpu::TextureDescriptor dstDescriptor =
        CreateTextureDescriptor(3, 1,
                                wgpu::TextureUsage::CopyDst | wgpu::TextureUsage::CopySrc |
                                    wgpu::TextureUsage::TextureBinding,
                                utils::kBCFormats[0]);
    wgpu::Texture dstTexture = device.CreateTexture(&dstDescriptor);

    wgpu::ImageCopyTexture dstImageCopyTexture =
        utils::CreateImageCopyTexture(dstTexture, kViewMipLevel, {0, 0, 0});

    wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
    encoder.CopyTextureToTexture(&srcImageCopyTexture, &dstImageCopyTexture, &copyExtent3D);
    wgpu::CommandBuffer commands = encoder.Finish();
    // expect 1 lazy clear count since the dstTexture needs to be lazy cleared when we only copy to
    // half texture
    EXPECT_LAZY_CLEAR(1u, queue.Submit(1, &commands));

    SampleCompressedTextureAndVerifyColor(dstTexture, dstDescriptor, copyExtent3D,
                                          {kActualSizeAtLevel / 2, kActualSizeAtLevel, 1},
                                          kViewMipLevel, 0, true);
}

// Test uploading then reading back from a 2D array compressed texture.
// This is a regression test for a bug where the final destination buffer
// was considered fully initialized even though there was a 256-byte
// stride between images.
TEST_P(CompressedTextureZeroInitTest, Copy2DArrayCompressedB2T2B) {
    // TODO(crbug.com/dawn/643): diagnose and fix this failure on OpenGL.
    DAWN_SUPPRESS_TEST_IF(IsOpenGL() || IsOpenGLES());

    // create srcTexture with data
    wgpu::TextureDescriptor textureDescriptor = CreateTextureDescriptor(
        4, 5, wgpu::TextureUsage::CopySrc | wgpu::TextureUsage::CopyDst, utils::kBCFormats[0]);
    textureDescriptor.size = {8, 8, 5};
    wgpu::Texture srcTexture = device.CreateTexture(&textureDescriptor);

    uint32_t mipLevel = 2;
    wgpu::Extent3D copyExtent3D = {4, 4, 5};

    uint32_t copyWidthInBlock = copyExtent3D.width / kFormatBlockByteSize;
    uint32_t copyHeightInBlock = copyExtent3D.height / kFormatBlockByteSize;
    uint32_t copyRowsPerImage = copyHeightInBlock;
    uint32_t copyBytesPerRow =
        Align(copyWidthInBlock * utils::GetTexelBlockSizeInBytes(textureDescriptor.format),
              kTextureBytesPerRowAlignment);

    // Generate data to upload
    std::vector<uint8_t> data(utils::RequiredBytesInCopy(copyBytesPerRow, copyRowsPerImage,
                                                         copyExtent3D, textureDescriptor.format));
    for (size_t i = 0; i < data.size(); ++i) {
        data[i] = i % 255;
    }

    // Copy texture data from a staging buffer to the destination texture.
    wgpu::Buffer stagingBuffer =
        utils::CreateBufferFromData(device, data.data(), data.size(), wgpu::BufferUsage::CopySrc);
    wgpu::ImageCopyBuffer imageCopyBufferSrc =
        utils::CreateImageCopyBuffer(stagingBuffer, 0, copyBytesPerRow, copyRowsPerImage);

    wgpu::ImageCopyTexture imageCopyTexture =
        utils::CreateImageCopyTexture(srcTexture, mipLevel, {0, 0, 0});

    {
        wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
        encoder.CopyBufferToTexture(&imageCopyBufferSrc, &imageCopyTexture, &copyExtent3D);
        wgpu::CommandBuffer copy = encoder.Finish();
        EXPECT_LAZY_CLEAR(0u, queue.Submit(1, &copy));
    }

    // Create a buffer to read back the data. It is the same size as the upload buffer.
    wgpu::BufferDescriptor readbackDesc = {};
    readbackDesc.size = data.size();
    readbackDesc.usage = wgpu::BufferUsage::CopySrc | wgpu::BufferUsage::CopyDst;
    wgpu::Buffer readbackBuffer = device.CreateBuffer(&readbackDesc);

    // Copy the texture to the readback buffer.
    wgpu::ImageCopyBuffer imageCopyBufferDst =
        utils::CreateImageCopyBuffer(readbackBuffer, 0, copyBytesPerRow, copyRowsPerImage);
    {
        wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
        encoder.CopyTextureToBuffer(&imageCopyTexture, &imageCopyBufferDst, &copyExtent3D);
        wgpu::CommandBuffer copy = encoder.Finish();

        // Expect a lazy clear because the padding in the copy is not touched.
        EXPECT_LAZY_CLEAR(1u, queue.Submit(1, &copy));
    }

    // Generate expected data. It is the same as the upload data, but padding is zero.
    std::vector<uint8_t> expected(data.size(), 0);
    for (uint32_t z = 0; z < copyExtent3D.depthOrArrayLayers; ++z) {
        for (uint32_t y = 0; y < copyHeightInBlock; ++y) {
            memcpy(&expected[copyBytesPerRow * y + copyBytesPerRow * copyRowsPerImage * z],
                   &data[copyBytesPerRow * y + copyBytesPerRow * copyRowsPerImage * z],
                   copyWidthInBlock * utils::GetTexelBlockSizeInBytes(textureDescriptor.format));
        }
    }
    // Check final contents
    EXPECT_BUFFER_U8_RANGE_EQ(expected.data(), readbackBuffer, 0, expected.size());
}

DAWN_INSTANTIATE_TEST(CompressedTextureZeroInitTest,
                      D3D12Backend({"nonzero_clear_resources_on_creation_for_testing"}),
                      MetalBackend({"nonzero_clear_resources_on_creation_for_testing"}),
                      OpenGLBackend({"nonzero_clear_resources_on_creation_for_testing"}),
                      OpenGLESBackend({"nonzero_clear_resources_on_creation_for_testing"}),
                      VulkanBackend({"nonzero_clear_resources_on_creation_for_testing"}));
