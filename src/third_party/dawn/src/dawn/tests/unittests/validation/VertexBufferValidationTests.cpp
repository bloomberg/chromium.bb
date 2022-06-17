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

#include <array>

#include "dawn/tests/unittests/validation/ValidationTest.h"

#include "dawn/utils/ComboRenderBundleEncoderDescriptor.h"
#include "dawn/utils/ComboRenderPipelineDescriptor.h"
#include "dawn/utils/WGPUHelpers.h"

class VertexBufferValidationTest : public ValidationTest {
  protected:
    void SetUp() override {
        ValidationTest::SetUp();

        // Placeholder vertex shader module
        vsModule = utils::CreateShaderModule(device, R"(
            @vertex fn main() -> @builtin(position) vec4<f32> {
                return vec4<f32>(0.0, 0.0, 0.0, 0.0);
            })");
        fsModule = utils::CreateShaderModule(device, R"(
            @fragment fn main() -> @location(0) vec4<f32> {
                return vec4<f32>(0.0, 1.0, 0.0, 1.0);
            })");
    }

    wgpu::Buffer MakeVertexBuffer() {
        wgpu::BufferDescriptor descriptor;
        descriptor.size = 256;
        descriptor.usage = wgpu::BufferUsage::Vertex;

        return device.CreateBuffer(&descriptor);
    }

    wgpu::ShaderModule MakeVertexShader(unsigned int bufferCount) {
        std::ostringstream vs;
        vs << "@vertex fn main(\n";
        for (unsigned int i = 0; i < bufferCount; ++i) {
            // TODO(cwallez@chromium.org): remove this special handling of 0 once Tint supports
            // trailing commas in argument lists.
            if (i != 0) {
                vs << ", ";
            }
            vs << "@location(" << i << ") a_position" << i << " : vec3<f32>\n";
        }
        vs << ") -> @builtin(position) vec4<f32> {";

        vs << "return vec4<f32>(";
        for (unsigned int i = 0; i < bufferCount; ++i) {
            vs << "a_position" << i;
            if (i != bufferCount - 1) {
                vs << " + ";
            }
        }
        vs << ", 1.0);";

        vs << "}\n";

        return utils::CreateShaderModule(device, vs.str().c_str());
    }

    wgpu::RenderPipeline MakeRenderPipeline(const wgpu::ShaderModule& vsModule,
                                            const utils::ComboVertexState& state) {
        utils::ComboRenderPipelineDescriptor descriptor;
        descriptor.vertex.module = vsModule;
        descriptor.cFragment.module = fsModule;

        descriptor.vertex.bufferCount = state.vertexBufferCount;
        descriptor.vertex.buffers = &state.cVertexBuffers[0];

        return device.CreateRenderPipeline(&descriptor);
    }

    wgpu::RenderPipeline MakeRenderPipeline(const wgpu::ShaderModule& vsModule,
                                            unsigned int bufferCount) {
        utils::ComboRenderPipelineDescriptor descriptor;
        descriptor.vertex.module = vsModule;
        descriptor.cFragment.module = fsModule;

        for (unsigned int i = 0; i < bufferCount; ++i) {
            descriptor.cBuffers[i].attributeCount = 1;
            descriptor.cBuffers[i].attributes = &descriptor.cAttributes[i];
            descriptor.cAttributes[i].shaderLocation = i;
            descriptor.cAttributes[i].format = wgpu::VertexFormat::Float32x3;
        }
        descriptor.vertex.bufferCount = bufferCount;

        return device.CreateRenderPipeline(&descriptor);
    }

    wgpu::ShaderModule vsModule;
    wgpu::ShaderModule fsModule;
};

// Check that vertex buffers still count as bound if we switch the pipeline.
TEST_F(VertexBufferValidationTest, VertexBuffersInheritedBetweenPipelines) {
    PlaceholderRenderPass renderPass(device);
    wgpu::ShaderModule vsModule2 = MakeVertexShader(2);
    wgpu::ShaderModule vsModule1 = MakeVertexShader(1);

    wgpu::RenderPipeline pipeline2 = MakeRenderPipeline(vsModule2, 2);
    wgpu::RenderPipeline pipeline1 = MakeRenderPipeline(vsModule1, 1);

    wgpu::Buffer vertexBuffer1 = MakeVertexBuffer();
    wgpu::Buffer vertexBuffer2 = MakeVertexBuffer();

    // Check failure when vertex buffer is not set
    wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
    {
        wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&renderPass);
        pass.SetPipeline(pipeline1);
        pass.Draw(3);
        pass.End();
    }
    ASSERT_DEVICE_ERROR(encoder.Finish());

    // Check success when vertex buffer is inherited from previous pipeline
    encoder = device.CreateCommandEncoder();
    {
        wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&renderPass);
        pass.SetPipeline(pipeline2);
        pass.SetVertexBuffer(0, vertexBuffer1);
        pass.SetVertexBuffer(1, vertexBuffer2);
        pass.Draw(3);
        pass.SetPipeline(pipeline1);
        pass.Draw(3);
        pass.End();
    }
    encoder.Finish();
}

// Check that vertex buffers that are set are reset between render passes.
TEST_F(VertexBufferValidationTest, VertexBuffersNotInheritedBetweenRenderPasses) {
    PlaceholderRenderPass renderPass(device);
    wgpu::ShaderModule vsModule2 = MakeVertexShader(2);
    wgpu::ShaderModule vsModule1 = MakeVertexShader(1);

    wgpu::RenderPipeline pipeline2 = MakeRenderPipeline(vsModule2, 2);
    wgpu::RenderPipeline pipeline1 = MakeRenderPipeline(vsModule1, 1);

    wgpu::Buffer vertexBuffer1 = MakeVertexBuffer();
    wgpu::Buffer vertexBuffer2 = MakeVertexBuffer();

    // Check success when vertex buffer is set for each render pass
    wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
    {
        wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&renderPass);
        pass.SetPipeline(pipeline2);
        pass.SetVertexBuffer(0, vertexBuffer1);
        pass.SetVertexBuffer(1, vertexBuffer2);
        pass.Draw(3);
        pass.End();
    }
    {
        wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&renderPass);
        pass.SetPipeline(pipeline1);
        pass.SetVertexBuffer(0, vertexBuffer1);
        pass.Draw(3);
        pass.End();
    }
    encoder.Finish();

    // Check failure because vertex buffer is not inherited in second subpass
    encoder = device.CreateCommandEncoder();
    {
        wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&renderPass);
        pass.SetPipeline(pipeline2);
        pass.SetVertexBuffer(0, vertexBuffer1);
        pass.SetVertexBuffer(1, vertexBuffer2);
        pass.Draw(3);
        pass.End();
    }
    {
        wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&renderPass);
        pass.SetPipeline(pipeline1);
        pass.Draw(3);
        pass.End();
    }
    ASSERT_DEVICE_ERROR(encoder.Finish());
}

// Check validation of the vertex buffer slot for OOB.
TEST_F(VertexBufferValidationTest, VertexBufferSlotValidation) {
    wgpu::Buffer buffer = MakeVertexBuffer();

    PlaceholderRenderPass renderPass(device);

    // Control case: using the last vertex buffer slot in render passes is ok.
    {
        wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
        wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&renderPass);
        pass.SetVertexBuffer(kMaxVertexBuffers - 1, buffer, 0);
        pass.End();
        encoder.Finish();
    }

    // Error case: using past the last vertex buffer slot in render pass fails.
    {
        wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
        wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&renderPass);
        pass.SetVertexBuffer(kMaxVertexBuffers, buffer, 0);
        pass.End();
        ASSERT_DEVICE_ERROR(encoder.Finish());
    }

    utils::ComboRenderBundleEncoderDescriptor renderBundleDesc = {};
    renderBundleDesc.colorFormatsCount = 1;
    renderBundleDesc.cColorFormats[0] = wgpu::TextureFormat::RGBA8Unorm;

    // Control case: using the last vertex buffer slot in render bundles is ok.
    {
        wgpu::RenderBundleEncoder encoder = device.CreateRenderBundleEncoder(&renderBundleDesc);
        encoder.SetVertexBuffer(kMaxVertexBuffers - 1, buffer, 0);
        encoder.Finish();
    }

    // Error case: using past the last vertex buffer slot in render bundle fails.
    {
        wgpu::RenderBundleEncoder encoder = device.CreateRenderBundleEncoder(&renderBundleDesc);
        encoder.SetVertexBuffer(kMaxVertexBuffers, buffer, 0);
        ASSERT_DEVICE_ERROR(encoder.Finish());
    }
}

// Test that for OOB validation of vertex buffer offset and size.
TEST_F(VertexBufferValidationTest, VertexBufferOffsetOOBValidation) {
    wgpu::Buffer buffer = MakeVertexBuffer();

    PlaceholderRenderPass renderPass(device);
    // Control case, using the full buffer, with or without an explicit size is valid.
    {
        wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
        wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&renderPass);
        // Explicit size
        pass.SetVertexBuffer(0, buffer, 0, 256);
        // Implicit size
        pass.SetVertexBuffer(0, buffer, 0, wgpu::kWholeSize);
        pass.SetVertexBuffer(0, buffer, 256 - 4, wgpu::kWholeSize);
        pass.SetVertexBuffer(0, buffer, 4, wgpu::kWholeSize);
        // Implicit size of zero
        pass.SetVertexBuffer(0, buffer, 256, wgpu::kWholeSize);
        pass.End();
        encoder.Finish();
    }

    // Bad case, offset + size is larger than the buffer
    {
        wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
        wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&renderPass);
        pass.SetVertexBuffer(0, buffer, 4, 256);
        pass.End();
        ASSERT_DEVICE_ERROR(encoder.Finish());
    }

    // Bad case, size is 0 but the offset is larger than the buffer
    {
        wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
        wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&renderPass);
        pass.SetVertexBuffer(0, buffer, 256 + 4, 0);
        pass.End();
        ASSERT_DEVICE_ERROR(encoder.Finish());
    }

    utils::ComboRenderBundleEncoderDescriptor renderBundleDesc = {};
    renderBundleDesc.colorFormatsCount = 1;
    renderBundleDesc.cColorFormats[0] = wgpu::TextureFormat::RGBA8Unorm;

    // Control case, using the full buffer, with or without an explicit size is valid.
    {
        wgpu::RenderBundleEncoder encoder = device.CreateRenderBundleEncoder(&renderBundleDesc);
        // Explicit size
        encoder.SetVertexBuffer(0, buffer, 0, 256);
        // Implicit size
        encoder.SetVertexBuffer(0, buffer, 0, wgpu::kWholeSize);
        encoder.SetVertexBuffer(0, buffer, 256 - 4, wgpu::kWholeSize);
        encoder.SetVertexBuffer(0, buffer, 4, wgpu::kWholeSize);
        // Implicit size of zero
        encoder.SetVertexBuffer(0, buffer, 256, wgpu::kWholeSize);
        encoder.Finish();
    }

    // Bad case, offset + size is larger than the buffer
    {
        wgpu::RenderBundleEncoder encoder = device.CreateRenderBundleEncoder(&renderBundleDesc);
        encoder.SetVertexBuffer(0, buffer, 4, 256);
        ASSERT_DEVICE_ERROR(encoder.Finish());
    }

    // Bad case, size is 0 but the offset is larger than the buffer
    {
        wgpu::RenderBundleEncoder encoder = device.CreateRenderBundleEncoder(&renderBundleDesc);
        encoder.SetVertexBuffer(0, buffer, 256 + 4, 0);
        ASSERT_DEVICE_ERROR(encoder.Finish());
    }
}

// Check that the vertex buffer must have the Vertex usage.
TEST_F(VertexBufferValidationTest, InvalidUsage) {
    wgpu::Buffer vertexBuffer = MakeVertexBuffer();
    wgpu::Buffer indexBuffer =
        utils::CreateBufferFromData<uint32_t>(device, wgpu::BufferUsage::Index, {0, 0, 0});

    PlaceholderRenderPass renderPass(device);
    // Control case: using the vertex buffer is valid.
    {
        wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
        wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&renderPass);
        pass.SetVertexBuffer(0, vertexBuffer);
        pass.End();
        encoder.Finish();
    }
    // Error case: using the index buffer is an error.
    {
        wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
        wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&renderPass);
        pass.SetVertexBuffer(0, indexBuffer);
        pass.End();
        ASSERT_DEVICE_ERROR(encoder.Finish());
    }

    utils::ComboRenderBundleEncoderDescriptor renderBundleDesc = {};
    renderBundleDesc.colorFormatsCount = 1;
    renderBundleDesc.cColorFormats[0] = wgpu::TextureFormat::RGBA8Unorm;
    // Control case: using the vertex buffer is valid.
    {
        wgpu::RenderBundleEncoder encoder = device.CreateRenderBundleEncoder(&renderBundleDesc);
        encoder.SetVertexBuffer(0, vertexBuffer);
        encoder.Finish();
    }
    // Error case: using the index buffer is an error.
    {
        wgpu::RenderBundleEncoder encoder = device.CreateRenderBundleEncoder(&renderBundleDesc);
        encoder.SetVertexBuffer(0, indexBuffer);
        ASSERT_DEVICE_ERROR(encoder.Finish());
    }
}

// Check the alignment constraint on the index buffer offset.
TEST_F(VertexBufferValidationTest, OffsetAlignment) {
    wgpu::Buffer vertexBuffer = MakeVertexBuffer();

    PlaceholderRenderPass renderPass(device);
    // Control cases: vertex buffer offset is a multiple of 4
    {
        wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
        wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&renderPass);
        pass.SetVertexBuffer(0, vertexBuffer, 0);
        pass.SetVertexBuffer(0, vertexBuffer, 4);
        pass.SetVertexBuffer(0, vertexBuffer, 12);
        pass.End();
        encoder.Finish();
    }

    // Error case: vertex buffer offset isn't a multiple of 4
    {
        wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
        wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&renderPass);
        pass.SetVertexBuffer(0, vertexBuffer, 2);
        pass.End();
        ASSERT_DEVICE_ERROR(encoder.Finish());
    }
}

// Check vertex buffer stride requirements for draw command.
TEST_F(VertexBufferValidationTest, DrawStrideLimitsVertex) {
    PlaceholderRenderPass renderPass(device);

    // Create a buffer of size 28, containing 4 float32 elements, array stride size = 8
    // The last element doesn't have the full stride size
    wgpu::BufferDescriptor descriptor;
    descriptor.size = 28;
    descriptor.usage = wgpu::BufferUsage::Vertex;
    wgpu::Buffer vertexBuffer = device.CreateBuffer(&descriptor);

    // Vertex attribute offset is 0
    wgpu::RenderPipeline pipeline1;
    {
        utils::ComboVertexState state;
        state.vertexBufferCount = 1;
        state.cVertexBuffers[0].arrayStride = 8;
        state.cVertexBuffers[0].stepMode = wgpu::VertexStepMode::Vertex;
        state.cVertexBuffers[0].attributeCount = 1;
        state.cAttributes[0].offset = 0;

        pipeline1 = MakeRenderPipeline(vsModule, state);
    }

    // Vertex attribute offset is 4
    wgpu::RenderPipeline pipeline2;
    {
        utils::ComboVertexState state;
        state.vertexBufferCount = 1;
        state.cVertexBuffers[0].arrayStride = 8;
        state.cVertexBuffers[0].stepMode = wgpu::VertexStepMode::Vertex;
        state.cVertexBuffers[0].attributeCount = 1;
        state.cAttributes[0].offset = 4;

        pipeline2 = MakeRenderPipeline(vsModule, state);
    }

    // Control case: draw 3 elements, 3 * 8 = 24 <= 28, is valid anyway
    wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
    {
        wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&renderPass);
        pass.SetPipeline(pipeline1);
        pass.SetVertexBuffer(0, vertexBuffer);
        pass.Draw(3);
        pass.End();
    }
    encoder.Finish();

    // Valid: draw 3 elements with firstVertex == 1, (2 + 1) * 8 + 4 = 28 <= 28
    encoder = device.CreateCommandEncoder();
    {
        wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&renderPass);
        pass.SetPipeline(pipeline1);
        pass.SetVertexBuffer(0, vertexBuffer);
        pass.Draw(3, 0, 1, 0);
        pass.End();
    }
    encoder.Finish();

    // Valid: draw 3 elements with offset == 4, 4 + 3 * 8 = 24 <= 28
    encoder = device.CreateCommandEncoder();
    {
        wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&renderPass);
        pass.SetPipeline(pipeline2);
        pass.SetVertexBuffer(0, vertexBuffer);
        pass.Draw(3);
        pass.End();
    }
    encoder.Finish();

    // Valid: draw 4 elements, 4 * 8 = 32 > 28
    // But the last element does not require to have the full stride size
    // So 3 * 8 + 4 = 28 <= 28
    encoder = device.CreateCommandEncoder();
    {
        wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&renderPass);
        pass.SetPipeline(pipeline1);
        pass.SetVertexBuffer(0, vertexBuffer);
        pass.Draw(4);
        pass.End();
    }
    encoder.Finish();

    // Invalid: draw 4 elements with firstVertex == 1
    // It requires a buffer with size of (3 + 1) * 8 + 4 = 36 > 28
    encoder = device.CreateCommandEncoder();
    {
        wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&renderPass);
        pass.SetPipeline(pipeline1);
        pass.SetVertexBuffer(0, vertexBuffer);
        pass.Draw(4, 0, 1, 0);
        pass.End();
    }
    ASSERT_DEVICE_ERROR(encoder.Finish());

    // Invalid: draw 4 elements with offset == 4
    // It requires a buffer with size of 4 + 3 * 8 + 4 = 32 > 28
    encoder = device.CreateCommandEncoder();
    {
        wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&renderPass);
        pass.SetPipeline(pipeline2);
        pass.SetVertexBuffer(0, vertexBuffer);
        pass.Draw(4);
        pass.End();
    }
    ASSERT_DEVICE_ERROR(encoder.Finish());

    // Valid: stride count == 0
    encoder = device.CreateCommandEncoder();
    {
        wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&renderPass);
        pass.SetPipeline(pipeline2);
        pass.SetVertexBuffer(0, vertexBuffer);
        pass.Draw(0);
        pass.End();
    }
    encoder.Finish();

    // Invalid: stride count == 4
    // It requires a buffer with size of 4 + 3 * 8 + 4 = 32 > 28
    encoder = device.CreateCommandEncoder();
    {
        wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&renderPass);
        pass.SetPipeline(pipeline2);
        pass.SetVertexBuffer(0, vertexBuffer);
        pass.Draw(0, 0, 4);
        pass.End();
    }
    ASSERT_DEVICE_ERROR(encoder.Finish());
}

// Check instance buffer stride requirements with instanced attributes for draw command.
TEST_F(VertexBufferValidationTest, DrawStrideLimitsInstance) {
    PlaceholderRenderPass renderPass(device);

    // Create a buffer of size 28, containing 4 float32 elements, array stride size = 8
    // The last element doesn't have the full stride size
    wgpu::BufferDescriptor descriptor;
    descriptor.size = 28;
    descriptor.usage = wgpu::BufferUsage::Vertex;
    wgpu::Buffer vertexBuffer = device.CreateBuffer(&descriptor);

    // Vertex attribute offset is 0
    wgpu::RenderPipeline pipeline1;
    {
        utils::ComboVertexState state;
        state.vertexBufferCount = 1;
        state.cVertexBuffers[0].arrayStride = 8;
        state.cVertexBuffers[0].stepMode = wgpu::VertexStepMode::Instance;
        state.cVertexBuffers[0].attributeCount = 1;
        state.cAttributes[0].offset = 0;

        pipeline1 = MakeRenderPipeline(vsModule, state);
    }

    // Vertex attribute offset is 4
    wgpu::RenderPipeline pipeline2;
    {
        utils::ComboVertexState state;
        state.vertexBufferCount = 1;
        state.cVertexBuffers[0].arrayStride = 8;
        state.cVertexBuffers[0].stepMode = wgpu::VertexStepMode::Instance;
        state.cVertexBuffers[0].attributeCount = 1;
        state.cAttributes[0].offset = 4;

        pipeline2 = MakeRenderPipeline(vsModule, state);
    }

    // Control case: draw 3 instances, 3 * 8 = 24 <= 28, is valid anyway
    wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
    {
        wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&renderPass);
        pass.SetPipeline(pipeline1);
        pass.SetVertexBuffer(0, vertexBuffer);
        pass.Draw(1, 3);
        pass.End();
    }
    encoder.Finish();

    // Valid: draw 3 instances with firstInstance == 1, (2 + 1) * 8 + 4 = 28 <= 28
    encoder = device.CreateCommandEncoder();
    {
        wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&renderPass);
        pass.SetPipeline(pipeline1);
        pass.SetVertexBuffer(0, vertexBuffer);
        pass.Draw(1, 3, 0, 1);
        pass.End();
    }
    encoder.Finish();

    // Valid: draw 3 instances with offset == 4, 4 + 3 * 8 = 24 <= 28
    encoder = device.CreateCommandEncoder();
    {
        wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&renderPass);
        pass.SetPipeline(pipeline2);
        pass.SetVertexBuffer(0, vertexBuffer);
        pass.Draw(1, 3);
        pass.End();
    }
    encoder.Finish();

    // Valid: draw 4 instances, 4 * 8 = 32 > 28
    // But the last element does not require to have the full stride size
    // So 3 * 8 + 4 = 28 <= 28
    encoder = device.CreateCommandEncoder();
    {
        wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&renderPass);
        pass.SetPipeline(pipeline1);
        pass.SetVertexBuffer(0, vertexBuffer);
        pass.Draw(1, 4);
        pass.End();
    }
    encoder.Finish();

    // Invalid: draw 4 instances with firstInstance == 1
    // It requires a buffer with size of (3 + 1) * 8 + 4 = 36 > 28
    encoder = device.CreateCommandEncoder();
    {
        wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&renderPass);
        pass.SetPipeline(pipeline1);
        pass.SetVertexBuffer(0, vertexBuffer);
        pass.Draw(1, 4, 0, 1);
        pass.End();
    }
    ASSERT_DEVICE_ERROR(encoder.Finish());

    // Invalid: draw 4 instances with offset == 4
    // It requires a buffer with size of 4 + 3 * 8 + 4 = 32 > 28
    encoder = device.CreateCommandEncoder();
    {
        wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&renderPass);
        pass.SetPipeline(pipeline2);
        pass.SetVertexBuffer(0, vertexBuffer);
        pass.Draw(1, 4);
        pass.End();
    }
    ASSERT_DEVICE_ERROR(encoder.Finish());

    // Valid: stride count == 0
    encoder = device.CreateCommandEncoder();
    {
        wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&renderPass);
        pass.SetPipeline(pipeline2);
        pass.SetVertexBuffer(0, vertexBuffer);
        pass.Draw(1, 0);
        pass.End();
    }
    encoder.Finish();

    // Invalid, stride count == 4
    // It requires a buffer with size of 4 + 3 * 8 + 4 = 32 > 28
    encoder = device.CreateCommandEncoder();
    {
        wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&renderPass);
        pass.SetPipeline(pipeline2);
        pass.SetVertexBuffer(0, vertexBuffer);
        pass.Draw(1, 0, 0, 4);
        pass.End();
    }
    ASSERT_DEVICE_ERROR(encoder.Finish());
}

// Check vertex buffer stride requirements with instanced attributes for draw indexed command.
TEST_F(VertexBufferValidationTest, DrawIndexedStrideLimitsInstance) {
    PlaceholderRenderPass renderPass(device);

    // Create a buffer of size 28, containing 4 float32 elements, array stride size = 8
    // The last element doesn't have the full stride size
    wgpu::BufferDescriptor descriptor;
    descriptor.size = 28;
    descriptor.usage = wgpu::BufferUsage::Vertex;
    wgpu::Buffer vertexBuffer = device.CreateBuffer(&descriptor);

    wgpu::Buffer indexBuffer =
        utils::CreateBufferFromData<uint32_t>(device, wgpu::BufferUsage::Index, {0, 1, 2});

    // Vertex attribute offset is 0
    wgpu::RenderPipeline pipeline1;
    {
        utils::ComboVertexState state;
        state.vertexBufferCount = 1;
        state.cVertexBuffers[0].arrayStride = 8;
        state.cVertexBuffers[0].stepMode = wgpu::VertexStepMode::Instance;
        state.cVertexBuffers[0].attributeCount = 1;
        state.cAttributes[0].offset = 0;

        pipeline1 = MakeRenderPipeline(vsModule, state);
    }

    // Vertex attribute offset is 4
    wgpu::RenderPipeline pipeline2;
    {
        utils::ComboVertexState state;
        state.vertexBufferCount = 1;
        state.cVertexBuffers[0].arrayStride = 8;
        state.cVertexBuffers[0].stepMode = wgpu::VertexStepMode::Instance;
        state.cVertexBuffers[0].attributeCount = 1;
        state.cAttributes[0].offset = 4;

        pipeline2 = MakeRenderPipeline(vsModule, state);
    }

    // Control case: draw 3 instances, 3 * 8 = 24 <= 28, is valid anyway
    wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
    {
        wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&renderPass);
        pass.SetPipeline(pipeline1);
        pass.SetVertexBuffer(0, vertexBuffer);
        pass.SetIndexBuffer(indexBuffer, wgpu::IndexFormat::Uint32);
        pass.DrawIndexed(3, 3);
        pass.End();
    }
    encoder.Finish();

    // Valid: draw 3 instances with firstInstance == 1, (2 + 1) * 8 + 4 = 28 <= 28
    encoder = device.CreateCommandEncoder();
    {
        wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&renderPass);
        pass.SetPipeline(pipeline1);
        pass.SetVertexBuffer(0, vertexBuffer);
        pass.SetIndexBuffer(indexBuffer, wgpu::IndexFormat::Uint32);
        pass.Draw(3, 3, 0, 1);
        pass.End();
    }
    encoder.Finish();

    // Valid: draw 3 instances with offset == 4, 4 + 3 * 8 = 24 <= 28
    encoder = device.CreateCommandEncoder();
    {
        wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&renderPass);
        pass.SetPipeline(pipeline2);
        pass.SetVertexBuffer(0, vertexBuffer);
        pass.SetIndexBuffer(indexBuffer, wgpu::IndexFormat::Uint32);
        pass.Draw(3, 3);
        pass.End();
    }
    encoder.Finish();

    // Valid: draw 4 instances, 4 * 8 = 32 > 28
    // But the last element does not require to have the full stride size
    // So 3 * 8 + 4 = 28 <= 28
    encoder = device.CreateCommandEncoder();
    {
        wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&renderPass);
        pass.SetPipeline(pipeline1);
        pass.SetVertexBuffer(0, vertexBuffer);
        pass.SetIndexBuffer(indexBuffer, wgpu::IndexFormat::Uint32);
        pass.Draw(3, 4);
        pass.End();
    }
    encoder.Finish();

    // Invalid: draw 4 instances with firstInstance == 1
    // It requires a buffer with size of (3 + 1) * 8 + 4 = 36 > 28
    encoder = device.CreateCommandEncoder();
    {
        wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&renderPass);
        pass.SetPipeline(pipeline1);
        pass.SetVertexBuffer(0, vertexBuffer);
        pass.SetIndexBuffer(indexBuffer, wgpu::IndexFormat::Uint32);
        pass.Draw(3, 4, 0, 1);
        pass.End();
    }
    ASSERT_DEVICE_ERROR(encoder.Finish());

    // Invalid: draw 4 instances with offset == 4
    // It requires a buffer with size of 4 + 3 * 8 + 4 = 32 > 28
    encoder = device.CreateCommandEncoder();
    {
        wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&renderPass);
        pass.SetPipeline(pipeline2);
        pass.SetVertexBuffer(0, vertexBuffer);
        pass.SetIndexBuffer(indexBuffer, wgpu::IndexFormat::Uint32);
        pass.Draw(3, 4);
        pass.End();
    }
    ASSERT_DEVICE_ERROR(encoder.Finish());

    // Valid: stride count == 0
    encoder = device.CreateCommandEncoder();
    {
        wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&renderPass);
        pass.SetPipeline(pipeline2);
        pass.SetVertexBuffer(0, vertexBuffer);
        pass.Draw(3, 0);
        pass.End();
    }
    encoder.Finish();

    // Invalid, stride count == 4
    // It requires a buffer with size of 4 + 3 * 8 + 4 = 32 > 28
    encoder = device.CreateCommandEncoder();
    {
        wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&renderPass);
        pass.SetPipeline(pipeline2);
        pass.SetVertexBuffer(0, vertexBuffer);
        pass.SetIndexBuffer(indexBuffer, wgpu::IndexFormat::Uint32);
        pass.Draw(3, 0, 0, 4);
        pass.End();
    }
    ASSERT_DEVICE_ERROR(encoder.Finish());
}

// Check last stride is computed correctly for vertex buffer with multiple attributes.
TEST_F(VertexBufferValidationTest, DrawStrideLimitsVertexMultipleAttributes) {
    PlaceholderRenderPass renderPass(device);

    // Create a buffer of size 44, array stride size = 12
    wgpu::BufferDescriptor descriptor;
    descriptor.size = 44;
    descriptor.usage = wgpu::BufferUsage::Vertex;
    wgpu::Buffer vertexBuffer = device.CreateBuffer(&descriptor);

    // lastStride = attribute[1].offset + sizeof(attribute[1].format) = 8
    wgpu::RenderPipeline pipeline1;
    {
        utils::ComboVertexState state;
        state.vertexBufferCount = 1;
        state.cVertexBuffers[0].arrayStride = 12;
        state.cVertexBuffers[0].stepMode = wgpu::VertexStepMode::Vertex;
        state.cVertexBuffers[0].attributeCount = 2;
        state.cAttributes[0].format = wgpu::VertexFormat::Float32;
        state.cAttributes[0].offset = 0;
        state.cAttributes[0].shaderLocation = 0;
        state.cAttributes[1].format = wgpu::VertexFormat::Float32;
        state.cAttributes[1].offset = 4;
        state.cAttributes[1].shaderLocation = 1;

        pipeline1 = MakeRenderPipeline(vsModule, state);
    }

    // lastStride = attribute[1].offset + sizeof(attribute[1].format) = 12
    wgpu::RenderPipeline pipeline2;
    {
        utils::ComboVertexState state;
        state.vertexBufferCount = 1;
        state.cVertexBuffers[0].arrayStride = 12;
        state.cVertexBuffers[0].stepMode = wgpu::VertexStepMode::Vertex;
        state.cVertexBuffers[0].attributeCount = 2;
        state.cAttributes[0].format = wgpu::VertexFormat::Float32;
        state.cAttributes[0].offset = 0;
        state.cAttributes[0].shaderLocation = 0;
        state.cAttributes[1].format = wgpu::VertexFormat::Float32x2;
        state.cAttributes[1].offset = 4;
        state.cAttributes[1].shaderLocation = 1;

        pipeline2 = MakeRenderPipeline(vsModule, state);
    }

    // Valid: draw 4 elements, last stride is 8, 3 * 12 + 8 = 44 <= 44
    wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
    {
        wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&renderPass);
        pass.SetPipeline(pipeline1);
        pass.SetVertexBuffer(0, vertexBuffer);
        pass.Draw(4);
        pass.End();
    }
    encoder.Finish();

    // Invalid: draw 4 elements, last stride is 12, 3 * 12 + 12 = 48 > 44
    encoder = device.CreateCommandEncoder();
    {
        wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&renderPass);
        pass.SetPipeline(pipeline2);
        pass.SetVertexBuffer(0, vertexBuffer);
        pass.Draw(4);
        pass.End();
    }
    ASSERT_DEVICE_ERROR(encoder.Finish());
}
