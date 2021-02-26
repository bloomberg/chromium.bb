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

#include "utils/ComboRenderPipelineDescriptor.h"
#include "utils/WGPUHelpers.h"

namespace {
    struct CreateReadyPipelineTask {
        wgpu::ComputePipeline computePipeline = nullptr;
        wgpu::RenderPipeline renderPipeline = nullptr;
        bool isCompleted = false;
        std::string message;
    };
}  // anonymous namespace

class CreateReadyPipelineTest : public DawnTest {
  protected:
    CreateReadyPipelineTask task;
};

// Verify the basic use of CreateReadyComputePipeline works on all backends.
TEST_P(CreateReadyPipelineTest, BasicUseOfCreateReadyComputePipeline) {
    const char* computeShader = R"(
        #version 450
        layout(std140, set = 0, binding = 0) buffer SSBO { uint value; } ssbo;
        void main() {
            ssbo.value = 1u;
        })";

    wgpu::ComputePipelineDescriptor csDesc;
    csDesc.computeStage.module =
        utils::CreateShaderModule(device, utils::SingleShaderStage::Compute, computeShader);
    csDesc.computeStage.entryPoint = "main";

    device.CreateReadyComputePipeline(
        &csDesc,
        [](WGPUCreateReadyPipelineStatus status, WGPUComputePipeline returnPipeline,
           const char* message, void* userdata) {
            ASSERT_EQ(WGPUCreateReadyPipelineStatus::WGPUCreateReadyPipelineStatus_Success, status);

            CreateReadyPipelineTask* task = static_cast<CreateReadyPipelineTask*>(userdata);
            task->computePipeline = wgpu::ComputePipeline::Acquire(returnPipeline);
            task->isCompleted = true;
            task->message = message;
        },
        &task);

    wgpu::BufferDescriptor bufferDesc;
    bufferDesc.size = sizeof(uint32_t);
    bufferDesc.usage = wgpu::BufferUsage::Storage | wgpu::BufferUsage::CopySrc;
    wgpu::Buffer ssbo = device.CreateBuffer(&bufferDesc);

    wgpu::CommandBuffer commands;
    {
        wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
        wgpu::ComputePassEncoder pass = encoder.BeginComputePass();

        while (!task.isCompleted) {
            WaitABit();
        }
        ASSERT_TRUE(task.message.empty());
        ASSERT_NE(nullptr, task.computePipeline.Get());
        wgpu::BindGroup bindGroup =
            utils::MakeBindGroup(device, task.computePipeline.GetBindGroupLayout(0),
                                 {
                                     {0, ssbo, 0, sizeof(uint32_t)},
                                 });
        pass.SetBindGroup(0, bindGroup);
        pass.SetPipeline(task.computePipeline);

        pass.Dispatch(1);
        pass.EndPass();

        commands = encoder.Finish();
    }

    queue.Submit(1, &commands);

    constexpr uint32_t kExpected = 1u;
    EXPECT_BUFFER_U32_EQ(kExpected, ssbo, 0);
}

// Verify CreateReadyComputePipeline() works as expected when there is any error that happens during
// the creation of the compute pipeline. The SPEC requires that during the call of
// CreateReadyComputePipeline() any error won't be forwarded to the error scope / unhandled error
// callback.
TEST_P(CreateReadyPipelineTest, CreateComputePipelineFailed) {
    DAWN_SKIP_TEST_IF(IsDawnValidationSkipped());

    const char* computeShader = R"(
        #version 450
        layout(std140, set = 0, binding = 0) buffer SSBO { uint value; } ssbo;
        void main() {
            ssbo.value = 1u;
        })";

    wgpu::ComputePipelineDescriptor csDesc;
    csDesc.computeStage.module =
        utils::CreateShaderModule(device, utils::SingleShaderStage::Compute, computeShader);
    csDesc.computeStage.entryPoint = "main0";

    device.CreateReadyComputePipeline(
        &csDesc,
        [](WGPUCreateReadyPipelineStatus status, WGPUComputePipeline returnPipeline,
           const char* message, void* userdata) {
            ASSERT_EQ(WGPUCreateReadyPipelineStatus::WGPUCreateReadyPipelineStatus_Error, status);

            CreateReadyPipelineTask* task = static_cast<CreateReadyPipelineTask*>(userdata);
            task->computePipeline = wgpu::ComputePipeline::Acquire(returnPipeline);
            task->isCompleted = true;
            task->message = message;
        },
        &task);

    while (!task.isCompleted) {
        WaitABit();
    }

    ASSERT_FALSE(task.message.empty());
    ASSERT_EQ(nullptr, task.computePipeline.Get());
}

// Verify the basic use of CreateReadyRenderPipeline() works on all backends.
TEST_P(CreateReadyPipelineTest, BasicUseOfCreateReadyRenderPipeline) {
    constexpr wgpu::TextureFormat kRenderAttachmentFormat = wgpu::TextureFormat::RGBA8Unorm;

    const char* vertexShader = R"(
        #version 450
        void main() {
            gl_Position = vec4(0.f, 0.f, 0.f, 1.f);
            gl_PointSize = 1.0f;
        })";
    const char* fragmentShader = R"(
        #version 450
        layout(location = 0) out vec4 o_color;
        void main() {
            o_color = vec4(0.f, 1.f, 0.f, 1.f);
        })";

    utils::ComboRenderPipelineDescriptor renderPipelineDescriptor(device);
    wgpu::ShaderModule vsModule =
        utils::CreateShaderModule(device, utils::SingleShaderStage::Vertex, vertexShader);
    wgpu::ShaderModule fsModule =
        utils::CreateShaderModule(device, utils::SingleShaderStage::Fragment, fragmentShader);
    renderPipelineDescriptor.vertexStage.module = vsModule;
    renderPipelineDescriptor.cFragmentStage.module = fsModule;
    renderPipelineDescriptor.cColorStates[0].format = kRenderAttachmentFormat;
    renderPipelineDescriptor.primitiveTopology = wgpu::PrimitiveTopology::PointList;

    device.CreateReadyRenderPipeline(
        &renderPipelineDescriptor,
        [](WGPUCreateReadyPipelineStatus status, WGPURenderPipeline returnPipeline,
           const char* message, void* userdata) {
            ASSERT_EQ(WGPUCreateReadyPipelineStatus::WGPUCreateReadyPipelineStatus_Success, status);

            CreateReadyPipelineTask* task = static_cast<CreateReadyPipelineTask*>(userdata);
            task->renderPipeline = wgpu::RenderPipeline::Acquire(returnPipeline);
            task->isCompleted = true;
            task->message = message;
        },
        &task);

    wgpu::TextureDescriptor textureDescriptor;
    textureDescriptor.size = {1, 1, 1};
    textureDescriptor.format = kRenderAttachmentFormat;
    textureDescriptor.usage = wgpu::TextureUsage::RenderAttachment | wgpu::TextureUsage::CopySrc;
    wgpu::Texture outputTexture = device.CreateTexture(&textureDescriptor);

    utils::ComboRenderPassDescriptor renderPassDescriptor({outputTexture.CreateView()});
    renderPassDescriptor.cColorAttachments[0].loadOp = wgpu::LoadOp::Clear;
    renderPassDescriptor.cColorAttachments[0].clearColor = {1.f, 0.f, 0.f, 1.f};

    wgpu::CommandBuffer commands;
    {
        wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
        wgpu::RenderPassEncoder renderPassEncoder = encoder.BeginRenderPass(&renderPassDescriptor);

        while (!task.isCompleted) {
            WaitABit();
        }
        ASSERT_TRUE(task.message.empty());
        ASSERT_NE(nullptr, task.renderPipeline.Get());

        renderPassEncoder.SetPipeline(task.renderPipeline);
        renderPassEncoder.Draw(1);
        renderPassEncoder.EndPass();
        commands = encoder.Finish();
    }

    queue.Submit(1, &commands);

    EXPECT_PIXEL_RGBA8_EQ(RGBA8(0, 255, 0, 255), outputTexture, 0, 0);
}

// Verify CreateReadyRenderPipeline() works as expected when there is any error that happens during
// the creation of the render pipeline. The SPEC requires that during the call of
// CreateReadyRenderPipeline() any error won't be forwarded to the error scope / unhandled error
// callback.
TEST_P(CreateReadyPipelineTest, CreateRenderPipelineFailed) {
    DAWN_SKIP_TEST_IF(IsDawnValidationSkipped());

    constexpr wgpu::TextureFormat kRenderAttachmentFormat = wgpu::TextureFormat::Depth32Float;

    const char* vertexShader = R"(
        #version 450
        void main() {
            gl_Position = vec4(0.f, 0.f, 0.f, 1.f);
            gl_PointSize = 1.0f;
        })";
    const char* fragmentShader = R"(
        #version 450
        layout(location = 0) out vec4 o_color;
        void main() {
            o_color = vec4(0.f, 1.f, 0.f, 1.f);
        })";

    utils::ComboRenderPipelineDescriptor renderPipelineDescriptor(device);
    wgpu::ShaderModule vsModule =
        utils::CreateShaderModule(device, utils::SingleShaderStage::Vertex, vertexShader);
    wgpu::ShaderModule fsModule =
        utils::CreateShaderModule(device, utils::SingleShaderStage::Fragment, fragmentShader);
    renderPipelineDescriptor.vertexStage.module = vsModule;
    renderPipelineDescriptor.cFragmentStage.module = fsModule;
    renderPipelineDescriptor.cColorStates[0].format = kRenderAttachmentFormat;
    renderPipelineDescriptor.primitiveTopology = wgpu::PrimitiveTopology::PointList;

    device.CreateReadyRenderPipeline(
        &renderPipelineDescriptor,
        [](WGPUCreateReadyPipelineStatus status, WGPURenderPipeline returnPipeline,
           const char* message, void* userdata) {
            ASSERT_EQ(WGPUCreateReadyPipelineStatus::WGPUCreateReadyPipelineStatus_Error, status);

            CreateReadyPipelineTask* task = static_cast<CreateReadyPipelineTask*>(userdata);
            task->renderPipeline = wgpu::RenderPipeline::Acquire(returnPipeline);
            task->isCompleted = true;
            task->message = message;
        },
        &task);

    while (!task.isCompleted) {
        WaitABit();
    }

    ASSERT_FALSE(task.message.empty());
    ASSERT_EQ(nullptr, task.computePipeline.Get());
}

// Verify there is no error when the device is released before the callback of
// CreateReadyComputePipeline() is called.
TEST_P(CreateReadyPipelineTest, ReleaseDeviceBeforeCallbackOfCreateReadyComputePipeline) {
    const char* computeShader = R"(
        #version 450
        void main() {
        })";

    wgpu::ComputePipelineDescriptor csDesc;
    csDesc.computeStage.module =
        utils::CreateShaderModule(device, utils::SingleShaderStage::Compute, computeShader);
    csDesc.computeStage.entryPoint = "main";

    device.CreateReadyComputePipeline(
        &csDesc,
        [](WGPUCreateReadyPipelineStatus status, WGPUComputePipeline returnPipeline,
           const char* message, void* userdata) {
            ASSERT_EQ(WGPUCreateReadyPipelineStatus::WGPUCreateReadyPipelineStatus_DeviceDestroyed,
                      status);

            CreateReadyPipelineTask* task = static_cast<CreateReadyPipelineTask*>(userdata);
            task->computePipeline = wgpu::ComputePipeline::Acquire(returnPipeline);
            task->isCompleted = true;
            task->message = message;
        },
        &task);
}

// Verify there is no error when the device is released before the callback of
// CreateReadyRenderPipeline() is called.
TEST_P(CreateReadyPipelineTest, ReleaseDeviceBeforeCallbackOfCreateReadyRenderPipeline) {
    const char* vertexShader = R"(
        #version 450
        void main() {
            gl_Position = vec4(0.f, 0.f, 0.f, 1.f);
            gl_PointSize = 1.0f;
        })";
    const char* fragmentShader = R"(
        #version 450
        layout(location = 0) out vec4 o_color;
        void main() {
            o_color = vec4(0.f, 1.f, 0.f, 1.f);
        })";

    utils::ComboRenderPipelineDescriptor renderPipelineDescriptor(device);
    wgpu::ShaderModule vsModule =
        utils::CreateShaderModule(device, utils::SingleShaderStage::Vertex, vertexShader);
    wgpu::ShaderModule fsModule =
        utils::CreateShaderModule(device, utils::SingleShaderStage::Fragment, fragmentShader);
    renderPipelineDescriptor.vertexStage.module = vsModule;
    renderPipelineDescriptor.cFragmentStage.module = fsModule;
    renderPipelineDescriptor.cColorStates[0].format = wgpu::TextureFormat::RGBA8Unorm;
    renderPipelineDescriptor.primitiveTopology = wgpu::PrimitiveTopology::PointList;

    device.CreateReadyRenderPipeline(
        &renderPipelineDescriptor,
        [](WGPUCreateReadyPipelineStatus status, WGPURenderPipeline returnPipeline,
           const char* message, void* userdata) {
            ASSERT_EQ(WGPUCreateReadyPipelineStatus::WGPUCreateReadyPipelineStatus_DeviceDestroyed,
                      status);

            CreateReadyPipelineTask* task = static_cast<CreateReadyPipelineTask*>(userdata);
            task->renderPipeline = wgpu::RenderPipeline::Acquire(returnPipeline);
            task->isCompleted = true;
            task->message = message;
        },
        &task);
}

DAWN_INSTANTIATE_TEST(CreateReadyPipelineTest,
                      D3D12Backend(),
                      MetalBackend(),
                      OpenGLBackend(),
                      VulkanBackend());
