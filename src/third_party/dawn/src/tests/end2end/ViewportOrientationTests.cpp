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

#include "utils/DawnHelpers.h"

class ViewportOrientationTests : public DawnTest {};

// Test that the pixel in viewport coordinate (-1, -1) matches texel (0, 0)
TEST_P(ViewportOrientationTests, OriginAt0x0) {
    utils::BasicRenderPass renderPass = utils::CreateBasicRenderPass(device, 2, 2);

    dawn::ShaderModule vsModule = utils::CreateShaderModule(device, dawn::ShaderStage::Vertex, R"(
        #version 450
        void main() {
            gl_Position = vec4(-0.5f, -0.5f, 0.0f, 1.0f);
        })");

    dawn::ShaderModule fsModule = utils::CreateShaderModule(device, dawn::ShaderStage::Fragment, R"(
        #version 450
        layout(location = 0) out vec4 fragColor;
        void main() {
            fragColor = vec4(0.0f, 1.0f, 0.0f, 1.0f);
        })");

    dawn::RenderPipeline pipeline = device.CreateRenderPipelineBuilder()
        .SetColorAttachmentFormat(0, renderPass.colorFormat)
        .SetStage(dawn::ShaderStage::Vertex, vsModule, "main")
        .SetStage(dawn::ShaderStage::Fragment, fsModule, "main")
        .SetPrimitiveTopology(dawn::PrimitiveTopology::PointList)
        .GetResult();

    dawn::CommandBufferBuilder builder = device.CreateCommandBufferBuilder();
    {
        dawn::RenderPassEncoder pass = builder.BeginRenderPass(renderPass.renderPassInfo);
        pass.SetRenderPipeline(pipeline);
        pass.DrawArrays(1, 1, 0, 0);
        pass.EndPass();
    }

    dawn::CommandBuffer commands = builder.GetResult();
    queue.Submit(1, &commands);

    EXPECT_PIXEL_RGBA8_EQ(RGBA8(0, 255, 0, 255), renderPass.color, 0, 0);
    EXPECT_PIXEL_RGBA8_EQ(RGBA8(0, 0, 0, 0), renderPass.color, 0, 1);
    EXPECT_PIXEL_RGBA8_EQ(RGBA8(0, 0, 0, 0), renderPass.color, 1, 0);
    EXPECT_PIXEL_RGBA8_EQ(RGBA8(0, 0, 0, 0), renderPass.color, 1, 1);
}

DAWN_INSTANTIATE_TEST(ViewportOrientationTests, D3D12Backend, MetalBackend, OpenGLBackend, VulkanBackend)
