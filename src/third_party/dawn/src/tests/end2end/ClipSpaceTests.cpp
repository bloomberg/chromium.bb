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

#include "utils/ComboRenderPipelineDescriptor.h"
#include "utils/WGPUHelpers.h"

class ClipSpaceTest : public DawnTest {
  protected:
    wgpu::RenderPipeline CreatePipelineForTest() {
        utils::ComboRenderPipelineDescriptor pipelineDescriptor;

        // Draw two triangles:
        // 1. The depth value of the top-left one is >= 0.5
        // 2. The depth value of the bottom-right one is <= 0.5
        pipelineDescriptor.vertex.module = utils::CreateShaderModule(device, R"(
            [[stage(vertex)]]
            fn main([[builtin(vertex_index)]] VertexIndex : u32) -> [[builtin(position)]] vec4<f32> {
                var pos = array<vec3<f32>, 6>(
                    vec3<f32>(-1.0,  1.0, 1.0),
                    vec3<f32>(-1.0, -1.0, 0.5),
                    vec3<f32>( 1.0,  1.0, 0.5),
                    vec3<f32>( 1.0,  1.0, 0.5),
                    vec3<f32>(-1.0, -1.0, 0.5),
                    vec3<f32>( 1.0, -1.0, 0.0));
                return vec4<f32>(pos[VertexIndex], 1.0);
            })");

        pipelineDescriptor.cFragment.module = utils::CreateShaderModule(device, R"(
            [[stage(fragment)]] fn main() -> [[location(0)]] vec4<f32> {
               return vec4<f32>(1.0, 0.0, 0.0, 1.0);
            })");

        wgpu::DepthStencilState* depthStencil = pipelineDescriptor.EnableDepthStencil();
        depthStencil->depthCompare = wgpu::CompareFunction::LessEqual;

        return device.CreateRenderPipeline(&pipelineDescriptor);
    }

    wgpu::Texture Create2DTextureForTest(wgpu::TextureFormat format) {
        wgpu::TextureDescriptor textureDescriptor;
        textureDescriptor.dimension = wgpu::TextureDimension::e2D;
        textureDescriptor.format = format;
        textureDescriptor.usage =
            wgpu::TextureUsage::RenderAttachment | wgpu::TextureUsage::CopySrc;
        textureDescriptor.mipLevelCount = 1;
        textureDescriptor.sampleCount = 1;
        textureDescriptor.size = {kSize, kSize, 1};
        return device.CreateTexture(&textureDescriptor);
    }

    static constexpr uint32_t kSize = 4;
};

// Test that the clip space is correctly configured.
TEST_P(ClipSpaceTest, ClipSpace) {
    wgpu::Texture colorTexture = Create2DTextureForTest(wgpu::TextureFormat::RGBA8Unorm);
    wgpu::Texture depthStencilTexture =
        Create2DTextureForTest(wgpu::TextureFormat::Depth24PlusStencil8);

    utils::ComboRenderPassDescriptor renderPassDescriptor({colorTexture.CreateView()},
                                                          depthStencilTexture.CreateView());
    renderPassDescriptor.cColorAttachments[0].clearColor = {0.0, 1.0, 0.0, 1.0};
    renderPassDescriptor.cColorAttachments[0].loadOp = wgpu::LoadOp::Clear;

    // Clear the depth stencil attachment to 0.5f, so only the bottom-right triangle should be
    // drawn.
    renderPassDescriptor.cDepthStencilAttachmentInfo.clearDepth = 0.5f;
    renderPassDescriptor.cDepthStencilAttachmentInfo.depthLoadOp = wgpu::LoadOp::Clear;

    wgpu::CommandEncoder commandEncoder = device.CreateCommandEncoder();
    wgpu::RenderPassEncoder renderPass = commandEncoder.BeginRenderPass(&renderPassDescriptor);
    renderPass.SetPipeline(CreatePipelineForTest());
    renderPass.Draw(6);
    renderPass.EndPass();
    wgpu::CommandBuffer commandBuffer = commandEncoder.Finish();
    queue.Submit(1, &commandBuffer);

    EXPECT_PIXEL_RGBA8_EQ(RGBA8::kRed, colorTexture, kSize - 1, kSize - 1);
    EXPECT_PIXEL_RGBA8_EQ(RGBA8::kGreen, colorTexture, 0, 0);
}

DAWN_INSTANTIATE_TEST(ClipSpaceTest,
                      D3D12Backend(),
                      MetalBackend(),
                      OpenGLBackend(),
                      OpenGLESBackend(),
                      VulkanBackend());
