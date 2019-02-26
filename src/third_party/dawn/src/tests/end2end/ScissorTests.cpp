// Copyright 2018 The Dawn Authors
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

#include "utils/DawnHelpers.h"

class ScissorTest: public DawnTest {
  protected:
    dawn::RenderPipeline CreateQuadPipeline(dawn::TextureFormat format) {
        dawn::ShaderModule vsModule = utils::CreateShaderModule(device, dawn::ShaderStage::Vertex, R"(
            #version 450
            const vec2 pos[6] = vec2[6](
                vec2(-1.0f, -1.0f), vec2(-1.0f, 1.0f), vec2(1.0f, -1.0f),
                vec2(1.0f, 1.0f), vec2(-1.0f, 1.0f), vec2(1.0f, -1.0f)
            );
            void main() {
                gl_Position = vec4(pos[gl_VertexIndex], 0.5, 1.0);
            })");

        dawn::ShaderModule fsModule = utils::CreateShaderModule(device, dawn::ShaderStage::Fragment, R"(
            #version 450
            layout(location = 0) out vec4 fragColor;
            void main() {
                fragColor = vec4(0.0f, 1.0f, 0.0f, 1.0f);
            })");

        dawn::RenderPipeline pipeline = device.CreateRenderPipelineBuilder()
            .SetColorAttachmentFormat(0, format)
            .SetStage(dawn::ShaderStage::Vertex, vsModule, "main")
            .SetStage(dawn::ShaderStage::Fragment, fsModule, "main")
            .GetResult();

        return pipeline;
    }
};

// Test that by default the scissor test is disabled and the whole attachment can be drawn to.
TEST_P(ScissorTest, DefaultsToWholeRenderTarget) {
    utils::BasicRenderPass renderPass = utils::CreateBasicRenderPass(device, 100, 100);
    dawn::RenderPipeline pipeline = CreateQuadPipeline(renderPass.colorFormat);

    dawn::CommandBufferBuilder builder = device.CreateCommandBufferBuilder();
    {
        dawn::RenderPassEncoder pass = builder.BeginRenderPass(renderPass.renderPassInfo);
        pass.SetRenderPipeline(pipeline);
        pass.DrawArrays(6, 1, 0, 0);
        pass.EndPass();
    }

    dawn::CommandBuffer commands = builder.GetResult();
    queue.Submit(1, &commands);

    EXPECT_PIXEL_RGBA8_EQ(RGBA8(0, 255, 0, 255), renderPass.color, 0, 0);
    EXPECT_PIXEL_RGBA8_EQ(RGBA8(0, 255, 0, 255), renderPass.color, 0, 99);
    EXPECT_PIXEL_RGBA8_EQ(RGBA8(0, 255, 0, 255), renderPass.color, 99, 0);
    EXPECT_PIXEL_RGBA8_EQ(RGBA8(0, 255, 0, 255), renderPass.color, 99, 99);
}

// Test setting the scissor to something larger than the attachments.
TEST_P(ScissorTest, LargerThanAttachment) {
    utils::BasicRenderPass renderPass = utils::CreateBasicRenderPass(device, 100, 100);
    dawn::RenderPipeline pipeline = CreateQuadPipeline(renderPass.colorFormat);

    dawn::CommandBufferBuilder builder = device.CreateCommandBufferBuilder();
    {
        dawn::RenderPassEncoder pass = builder.BeginRenderPass(renderPass.renderPassInfo);
        pass.SetRenderPipeline(pipeline);
        pass.SetScissorRect(0, 0, 200, 200);
        pass.DrawArrays(6, 1, 0, 0);
        pass.EndPass();
    }

    dawn::CommandBuffer commands = builder.GetResult();
    queue.Submit(1, &commands);

    EXPECT_PIXEL_RGBA8_EQ(RGBA8(0, 255, 0, 255), renderPass.color, 0, 0);
    EXPECT_PIXEL_RGBA8_EQ(RGBA8(0, 255, 0, 255), renderPass.color, 0, 99);
    EXPECT_PIXEL_RGBA8_EQ(RGBA8(0, 255, 0, 255), renderPass.color, 99, 0);
    EXPECT_PIXEL_RGBA8_EQ(RGBA8(0, 255, 0, 255), renderPass.color, 99, 99);
}

// Test setting an empty scissor rect
TEST_P(ScissorTest, EmptyRect) {
    DAWN_SKIP_TEST_IF(IsMetal());
    DAWN_SKIP_TEST_IF(IsWindows() && IsVulkan() && IsIntel());

    utils::BasicRenderPass renderPass = utils::CreateBasicRenderPass(device, 2, 2);
    dawn::RenderPipeline pipeline = CreateQuadPipeline(renderPass.colorFormat);

    dawn::CommandBufferBuilder builder = device.CreateCommandBufferBuilder();
    {
        dawn::RenderPassEncoder pass = builder.BeginRenderPass(renderPass.renderPassInfo);
        pass.SetRenderPipeline(pipeline);
        pass.SetScissorRect(0, 0, 0, 0);
        pass.DrawArrays(6, 1, 0, 0);
        pass.EndPass();
    }

    dawn::CommandBuffer commands = builder.GetResult();
    queue.Submit(1, &commands);

    EXPECT_PIXEL_RGBA8_EQ(RGBA8(0, 0, 0, 0), renderPass.color, 0, 0);
    EXPECT_PIXEL_RGBA8_EQ(RGBA8(0, 0, 0, 0), renderPass.color, 0, 1);
    EXPECT_PIXEL_RGBA8_EQ(RGBA8(0, 0, 0, 0), renderPass.color, 1, 0);
    EXPECT_PIXEL_RGBA8_EQ(RGBA8(0, 0, 0, 0), renderPass.color, 1, 1);
}

// Test setting a partial scissor (not empty, not full attachment)
TEST_P(ScissorTest, PartialRect) {
    utils::BasicRenderPass renderPass = utils::CreateBasicRenderPass(device, 100, 100);
    dawn::RenderPipeline pipeline = CreateQuadPipeline(renderPass.colorFormat);

    constexpr uint32_t kX = 3;
    constexpr uint32_t kY = 7;
    constexpr uint32_t kW = 5;
    constexpr uint32_t kH = 13;

    dawn::CommandBufferBuilder builder = device.CreateCommandBufferBuilder();
    {
        dawn::RenderPassEncoder pass = builder.BeginRenderPass(renderPass.renderPassInfo);
        pass.SetRenderPipeline(pipeline);
        pass.SetScissorRect(kX, kY, kW, kH);
        pass.DrawArrays(6, 1, 0, 0);
        pass.EndPass();
    }

    dawn::CommandBuffer commands = builder.GetResult();
    queue.Submit(1, &commands);

    // Test the two opposite corners of the scissor box. With one pixel inside and on outside
    EXPECT_PIXEL_RGBA8_EQ(RGBA8(0, 0, 0, 0), renderPass.color, kX - 1, kY - 1);
    EXPECT_PIXEL_RGBA8_EQ(RGBA8(0, 255, 0, 255), renderPass.color, kX, kY);

    EXPECT_PIXEL_RGBA8_EQ(RGBA8(0, 0, 0, 0), renderPass.color, kX + kW, kY + kH);
    EXPECT_PIXEL_RGBA8_EQ(RGBA8(0, 255, 0, 255), renderPass.color, kX + kW - 1, kY + kH - 1);
}

// Test that the scissor setting doesn't get inherited between renderpasses
TEST_P(ScissorTest, NoInheritanceBetweenRenderPass) {
    utils::BasicRenderPass renderPass = utils::CreateBasicRenderPass(device, 100, 100);
    dawn::RenderPipeline pipeline = CreateQuadPipeline(renderPass.colorFormat);

    dawn::CommandBufferBuilder builder = device.CreateCommandBufferBuilder();
    // RenderPass 1 set the scissor
    {
        dawn::RenderPassEncoder pass = builder.BeginRenderPass(renderPass.renderPassInfo);
        pass.SetScissorRect(0, 0, 0, 0);
        pass.EndPass();
    }
    // RenderPass 2 draw a full quad, it shouldn't be scissored
    {
        dawn::RenderPassEncoder pass = builder.BeginRenderPass(renderPass.renderPassInfo);
        pass.SetRenderPipeline(pipeline);
        pass.DrawArrays(6, 1, 0, 0);
        pass.EndPass();
    }

    dawn::CommandBuffer commands = builder.GetResult();
    queue.Submit(1, &commands);

    EXPECT_PIXEL_RGBA8_EQ(RGBA8(0, 255, 0, 255), renderPass.color, 0, 0);
    EXPECT_PIXEL_RGBA8_EQ(RGBA8(0, 255, 0, 255), renderPass.color, 0, 99);
    EXPECT_PIXEL_RGBA8_EQ(RGBA8(0, 255, 0, 255), renderPass.color, 99, 0);
    EXPECT_PIXEL_RGBA8_EQ(RGBA8(0, 255, 0, 255), renderPass.color, 99, 99);
}

DAWN_INSTANTIATE_TEST(ScissorTest, D3D12Backend, MetalBackend, OpenGLBackend, VulkanBackend)
