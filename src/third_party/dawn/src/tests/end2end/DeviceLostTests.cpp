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

#include <gmock/gmock.h>
#include "tests/MockCallback.h"
#include "utils/ComboRenderPipelineDescriptor.h"
#include "utils/WGPUHelpers.h"

#include <cstring>

using namespace testing;

class MockDeviceLostCallback {
  public:
    MOCK_METHOD(void, Call, (const char* message, void* userdata));
};

static std::unique_ptr<MockDeviceLostCallback> mockDeviceLostCallback;
static void ToMockDeviceLostCallback(const char* message, void* userdata) {
    mockDeviceLostCallback->Call(message, userdata);
    DawnTestBase* self = static_cast<DawnTestBase*>(userdata);
    self->StartExpectDeviceError();
}

class MockFenceOnCompletionCallback {
  public:
    MOCK_METHOD(void, Call, (WGPUFenceCompletionStatus status, void* userdata));
};

static std::unique_ptr<MockFenceOnCompletionCallback> mockFenceOnCompletionCallback;
static void ToMockFenceOnCompletionFails(WGPUFenceCompletionStatus status, void* userdata) {
    EXPECT_EQ(WGPUFenceCompletionStatus_DeviceLost, status);
    mockFenceOnCompletionCallback->Call(status, userdata);
    mockFenceOnCompletionCallback = nullptr;
}
static void ToMockFenceOnCompletionSucceeds(WGPUFenceCompletionStatus status, void* userdata) {
    EXPECT_EQ(WGPUFenceCompletionStatus_Success, status);
    mockFenceOnCompletionCallback->Call(status, userdata);
    mockFenceOnCompletionCallback = nullptr;
}

class MockQueueWorkDoneCallback {
  public:
    MOCK_METHOD(void, Call, (WGPUQueueWorkDoneStatus status, void* userdata));
};

static std::unique_ptr<MockQueueWorkDoneCallback> mockQueueWorkDoneCallback;
static void ToMockQueueWorkDone(WGPUQueueWorkDoneStatus status, void* userdata) {
    mockQueueWorkDoneCallback->Call(status, userdata);
}

static const int fakeUserData = 0;

class DeviceLostTest : public DawnTest {
  protected:
    void SetUp() override {
        DawnTest::SetUp();
        DAWN_SKIP_TEST_IF(UsesWire());
        mockDeviceLostCallback = std::make_unique<MockDeviceLostCallback>();
        mockFenceOnCompletionCallback = std::make_unique<MockFenceOnCompletionCallback>();
        mockQueueWorkDoneCallback = std::make_unique<MockQueueWorkDoneCallback>();
    }

    void TearDown() override {
        mockDeviceLostCallback = nullptr;
        mockFenceOnCompletionCallback = nullptr;
        mockQueueWorkDoneCallback = nullptr;
        DawnTest::TearDown();
    }

    void SetCallbackAndLoseForTesting() {
        device.SetDeviceLostCallback(ToMockDeviceLostCallback, this);
        EXPECT_CALL(*mockDeviceLostCallback, Call(_, this)).Times(1);
        device.LoseForTesting();
    }

    static void MapFailCallback(WGPUBufferMapAsyncStatus status, void* userdata) {
        EXPECT_EQ(WGPUBufferMapAsyncStatus_DeviceLost, status);
        EXPECT_EQ(&fakeUserData, userdata);
    }
};

// Test that DeviceLostCallback is invoked when LostForTestimg is called
TEST_P(DeviceLostTest, DeviceLostCallbackIsCalled) {
    SetCallbackAndLoseForTesting();
}

// Test that submit fails when device is lost
TEST_P(DeviceLostTest, SubmitFails) {
    wgpu::CommandBuffer commands;
    wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
    commands = encoder.Finish();

    SetCallbackAndLoseForTesting();
    ASSERT_DEVICE_ERROR(queue.Submit(0, &commands));
}

// Test that CreateBindGroupLayout fails when device is lost
TEST_P(DeviceLostTest, CreateBindGroupLayoutFails) {
    SetCallbackAndLoseForTesting();

    wgpu::BindGroupLayoutEntry entry;
    entry.binding = 0;
    entry.visibility = wgpu::ShaderStage::None;
    entry.buffer.type = wgpu::BufferBindingType::Uniform;
    wgpu::BindGroupLayoutDescriptor descriptor;
    descriptor.entryCount = 1;
    descriptor.entries = &entry;
    ASSERT_DEVICE_ERROR(device.CreateBindGroupLayout(&descriptor));
}

// Test that GetBindGroupLayout fails when device is lost
TEST_P(DeviceLostTest, GetBindGroupLayoutFails) {
    wgpu::ShaderModule csModule = utils::CreateShaderModule(device, R"(
        [[block]] struct UniformBuffer {
            pos : vec4<f32>;
        };
        [[group(0), binding(0)]] var<uniform> ubo : UniformBuffer;
        [[stage(compute)]] fn main() {
        })");

    wgpu::ComputePipelineDescriptor descriptor;
    descriptor.layout = nullptr;
    descriptor.computeStage.module = csModule;
    descriptor.computeStage.entryPoint = "main";

    wgpu::ComputePipeline pipeline = device.CreateComputePipeline(&descriptor);

    SetCallbackAndLoseForTesting();
    ASSERT_DEVICE_ERROR(pipeline.GetBindGroupLayout(0).Get());
}

// Test that CreateBindGroup fails when device is lost
TEST_P(DeviceLostTest, CreateBindGroupFails) {
    SetCallbackAndLoseForTesting();

    wgpu::BindGroupEntry entry;
    entry.binding = 0;
    entry.sampler = nullptr;
    entry.textureView = nullptr;
    entry.buffer = nullptr;
    entry.offset = 0;
    entry.size = 0;

    wgpu::BindGroupDescriptor descriptor;
    descriptor.layout = nullptr;
    descriptor.entryCount = 1;
    descriptor.entries = &entry;
    ASSERT_DEVICE_ERROR(device.CreateBindGroup(&descriptor));
}

// Test that CreatePipelineLayout fails when device is lost
TEST_P(DeviceLostTest, CreatePipelineLayoutFails) {
    SetCallbackAndLoseForTesting();

    wgpu::PipelineLayoutDescriptor descriptor;
    descriptor.bindGroupLayoutCount = 0;
    descriptor.bindGroupLayouts = nullptr;
    ASSERT_DEVICE_ERROR(device.CreatePipelineLayout(&descriptor));
}

// Tests that CreateRenderBundleEncoder fails when device is lost
TEST_P(DeviceLostTest, CreateRenderBundleEncoderFails) {
    SetCallbackAndLoseForTesting();

    wgpu::RenderBundleEncoderDescriptor descriptor;
    descriptor.colorFormatsCount = 0;
    descriptor.colorFormats = nullptr;
    ASSERT_DEVICE_ERROR(device.CreateRenderBundleEncoder(&descriptor));
}

// Tests that CreateComputePipeline fails when device is lost
TEST_P(DeviceLostTest, CreateComputePipelineFails) {
    SetCallbackAndLoseForTesting();

    wgpu::ComputePipelineDescriptor descriptor = {};
    descriptor.layout = nullptr;
    descriptor.computeStage.module = nullptr;
    ASSERT_DEVICE_ERROR(device.CreateComputePipeline(&descriptor));
}

// Tests that CreateRenderPipeline fails when device is lost
TEST_P(DeviceLostTest, CreateRenderPipelineFails) {
    SetCallbackAndLoseForTesting();

    utils::ComboRenderPipelineDescriptor2 descriptor;
    ASSERT_DEVICE_ERROR(device.CreateRenderPipeline2(&descriptor));
}

// Tests that CreateSampler fails when device is lost
TEST_P(DeviceLostTest, CreateSamplerFails) {
    SetCallbackAndLoseForTesting();

    ASSERT_DEVICE_ERROR(device.CreateSampler());
}

// Tests that CreateShaderModule fails when device is lost
TEST_P(DeviceLostTest, CreateShaderModuleFails) {
    SetCallbackAndLoseForTesting();

    ASSERT_DEVICE_ERROR(utils::CreateShaderModule(device, R"(
        [[stage(fragment)]]
        fn main([[location(0)]] color : vec4<f32>) -> [[location(0)]] vec4<f32> {
            return color;
        })"));
}

// Tests that CreateSwapChain fails when device is lost
TEST_P(DeviceLostTest, CreateSwapChainFails) {
    SetCallbackAndLoseForTesting();

    wgpu::SwapChainDescriptor descriptor = {};
    ASSERT_DEVICE_ERROR(device.CreateSwapChain(nullptr, &descriptor));
}

// Tests that CreateTexture fails when device is lost
TEST_P(DeviceLostTest, CreateTextureFails) {
    SetCallbackAndLoseForTesting();

    wgpu::TextureDescriptor descriptor;
    descriptor.size.width = 4;
    descriptor.size.height = 4;
    descriptor.size.depthOrArrayLayers = 1;
    descriptor.mipLevelCount = 1;
    descriptor.dimension = wgpu::TextureDimension::e2D;
    descriptor.usage = wgpu::TextureUsage::RenderAttachment;

    ASSERT_DEVICE_ERROR(device.CreateTexture(&descriptor));
}

TEST_P(DeviceLostTest, TickFails) {
    SetCallbackAndLoseForTesting();
    ASSERT_DEVICE_ERROR(device.Tick());
}

// Test that CreateBuffer fails when device is lost
TEST_P(DeviceLostTest, CreateBufferFails) {
    SetCallbackAndLoseForTesting();

    wgpu::BufferDescriptor bufferDescriptor;
    bufferDescriptor.size = sizeof(float);
    bufferDescriptor.usage = wgpu::BufferUsage::CopySrc;
    ASSERT_DEVICE_ERROR(device.CreateBuffer(&bufferDescriptor));
}

// Test that buffer.MapAsync for writing fails after device is lost
TEST_P(DeviceLostTest, BufferMapAsyncFailsForWriting) {
    wgpu::BufferDescriptor bufferDescriptor;
    bufferDescriptor.size = 4;
    bufferDescriptor.usage = wgpu::BufferUsage::MapWrite;
    wgpu::Buffer buffer = device.CreateBuffer(&bufferDescriptor);

    SetCallbackAndLoseForTesting();
    ASSERT_DEVICE_ERROR(buffer.MapAsync(wgpu::MapMode::Write, 0, 4, MapFailCallback,
                                        const_cast<int*>(&fakeUserData)));
}

// Test that BufferMapAsync for writing calls back with device lost status when device lost after
// mapping
TEST_P(DeviceLostTest, BufferMapAsyncBeforeLossFailsForWriting) {
    wgpu::BufferDescriptor bufferDescriptor;
    bufferDescriptor.size = 4;
    bufferDescriptor.usage = wgpu::BufferUsage::MapWrite;
    wgpu::Buffer buffer = device.CreateBuffer(&bufferDescriptor);

    buffer.MapAsync(wgpu::MapMode::Write, 0, 4, MapFailCallback, const_cast<int*>(&fakeUserData));
    SetCallbackAndLoseForTesting();
}

// Test that buffer.Unmap fails after device is lost
TEST_P(DeviceLostTest, BufferUnmapFails) {
    wgpu::BufferDescriptor bufferDescriptor;
    bufferDescriptor.size = sizeof(float);
    bufferDescriptor.usage = wgpu::BufferUsage::MapWrite;
    bufferDescriptor.mappedAtCreation = true;
    wgpu::Buffer buffer = device.CreateBuffer(&bufferDescriptor);

    SetCallbackAndLoseForTesting();
    ASSERT_DEVICE_ERROR(buffer.Unmap());
}

// Test that mappedAtCreation fails after device is lost
TEST_P(DeviceLostTest, CreateBufferMappedAtCreationFails) {
    wgpu::BufferDescriptor bufferDescriptor;
    bufferDescriptor.size = sizeof(float);
    bufferDescriptor.usage = wgpu::BufferUsage::MapWrite;
    bufferDescriptor.mappedAtCreation = true;

    SetCallbackAndLoseForTesting();
    ASSERT_DEVICE_ERROR(device.CreateBuffer(&bufferDescriptor));
}

// Test that BufferMapAsync for reading fails after device is lost
TEST_P(DeviceLostTest, BufferMapAsyncFailsForReading) {
    wgpu::BufferDescriptor bufferDescriptor;
    bufferDescriptor.size = 4;
    bufferDescriptor.usage = wgpu::BufferUsage::MapRead | wgpu::BufferUsage::CopyDst;

    wgpu::Buffer buffer = device.CreateBuffer(&bufferDescriptor);

    SetCallbackAndLoseForTesting();
    ASSERT_DEVICE_ERROR(buffer.MapAsync(wgpu::MapMode::Read, 0, 4, MapFailCallback,
                                        const_cast<int*>(&fakeUserData)));
}

// Test that BufferMapAsync for reading calls back with device lost status when device lost after
// mapping
TEST_P(DeviceLostTest, BufferMapAsyncBeforeLossFailsForReading) {
    wgpu::BufferDescriptor bufferDescriptor;
    bufferDescriptor.size = sizeof(float);
    bufferDescriptor.usage = wgpu::BufferUsage::MapRead | wgpu::BufferUsage::CopyDst;

    wgpu::Buffer buffer = device.CreateBuffer(&bufferDescriptor);

    buffer.MapAsync(wgpu::MapMode::Read, 0, 4, MapFailCallback, const_cast<int*>(&fakeUserData));
    SetCallbackAndLoseForTesting();
}

// Test that WriteBuffer fails after device is lost
TEST_P(DeviceLostTest, WriteBufferFails) {
    wgpu::BufferDescriptor bufferDescriptor;
    bufferDescriptor.size = sizeof(float);
    bufferDescriptor.usage = wgpu::BufferUsage::MapRead | wgpu::BufferUsage::CopyDst;

    wgpu::Buffer buffer = device.CreateBuffer(&bufferDescriptor);

    SetCallbackAndLoseForTesting();
    float data = 12.0f;
    ASSERT_DEVICE_ERROR(queue.WriteBuffer(buffer, 0, &data, sizeof(data)));
}

// Test it's possible to GetMappedRange on a buffer created mapped after device loss
TEST_P(DeviceLostTest, GetMappedRange_CreateBufferMappedAtCreationAfterLoss) {
    SetCallbackAndLoseForTesting();

    wgpu::BufferDescriptor desc;
    desc.size = 4;
    desc.usage = wgpu::BufferUsage::CopySrc;
    desc.mappedAtCreation = true;
    ASSERT_DEVICE_ERROR(wgpu::Buffer buffer = device.CreateBuffer(&desc));

    ASSERT_NE(buffer.GetMappedRange(), nullptr);
}

// Test that device loss doesn't change the result of GetMappedRange, mappedAtCreation version.
TEST_P(DeviceLostTest, GetMappedRange_CreateBufferMappedAtCreationBeforeLoss) {
    wgpu::BufferDescriptor desc;
    desc.size = 4;
    desc.usage = wgpu::BufferUsage::CopySrc;
    desc.mappedAtCreation = true;
    wgpu::Buffer buffer = device.CreateBuffer(&desc);

    void* rangeBeforeLoss = buffer.GetMappedRange();
    SetCallbackAndLoseForTesting();

    ASSERT_NE(buffer.GetMappedRange(), nullptr);
    ASSERT_EQ(buffer.GetMappedRange(), rangeBeforeLoss);
}

// Test that device loss doesn't change the result of GetMappedRange, mapping for reading version.
TEST_P(DeviceLostTest, GetMappedRange_MapAsyncReading) {
    wgpu::BufferDescriptor desc;
    desc.size = 4;
    desc.usage = wgpu::BufferUsage::MapRead | wgpu::BufferUsage::CopyDst;
    wgpu::Buffer buffer = device.CreateBuffer(&desc);

    buffer.MapAsync(wgpu::MapMode::Read, 0, 4, nullptr, nullptr);
    queue.Submit(0, nullptr);

    const void* rangeBeforeLoss = buffer.GetConstMappedRange();
    SetCallbackAndLoseForTesting();

    ASSERT_NE(buffer.GetConstMappedRange(), nullptr);
    ASSERT_EQ(buffer.GetConstMappedRange(), rangeBeforeLoss);
}

// Test that device loss doesn't change the result of GetMappedRange, mapping for writing version.
TEST_P(DeviceLostTest, GetMappedRange_MapAsyncWriting) {
    wgpu::BufferDescriptor desc;
    desc.size = 4;
    desc.usage = wgpu::BufferUsage::MapWrite | wgpu::BufferUsage::CopySrc;
    wgpu::Buffer buffer = device.CreateBuffer(&desc);

    buffer.MapAsync(wgpu::MapMode::Write, 0, 4, nullptr, nullptr);
    queue.Submit(0, nullptr);

    const void* rangeBeforeLoss = buffer.GetConstMappedRange();
    SetCallbackAndLoseForTesting();

    ASSERT_NE(buffer.GetConstMappedRange(), nullptr);
    ASSERT_EQ(buffer.GetConstMappedRange(), rangeBeforeLoss);
}

// TODO mapasync read + resolve + loss getmappedrange != nullptr.
// TODO mapasync write + resolve + loss getmappedrange != nullptr.

// Test that Command Encoder Finish fails when device lost
TEST_P(DeviceLostTest, CommandEncoderFinishFails) {
    wgpu::CommandBuffer commands;
    wgpu::CommandEncoder encoder = device.CreateCommandEncoder();

    SetCallbackAndLoseForTesting();
    ASSERT_DEVICE_ERROR(encoder.Finish());
}

// Test that CreateFenceFails when device is lost
TEST_P(DeviceLostTest, CreateFenceFails) {
    SetCallbackAndLoseForTesting();

    EXPECT_DEPRECATION_WARNING(ASSERT_DEVICE_ERROR(queue.CreateFence()));
}

// Test that queue signal fails when device is lost
TEST_P(DeviceLostTest, QueueSignalFenceFails) {
    wgpu::Fence fence;
    EXPECT_DEPRECATION_WARNING(fence = queue.CreateFence());

    SetCallbackAndLoseForTesting();

    ASSERT_DEVICE_ERROR(queue.Signal(fence, 3));

    // callback should have device lost status
    EXPECT_CALL(*mockFenceOnCompletionCallback, Call(WGPUFenceCompletionStatus_DeviceLost, nullptr))
        .Times(1);
    ASSERT_DEVICE_ERROR(fence.OnCompletion(2u, ToMockFenceOnCompletionFails, nullptr));

    // completed value should not have changed from initial value
    EXPECT_EQ(fence.GetCompletedValue(), 0u);
}

// Test that Fence On Completion fails after device is lost
TEST_P(DeviceLostTest, FenceOnCompletionFails) {
    wgpu::Fence fence;
    EXPECT_DEPRECATION_WARNING(fence = queue.CreateFence());

    queue.Signal(fence, 2);

    SetCallbackAndLoseForTesting();
    // callback should have device lost status
    EXPECT_CALL(*mockFenceOnCompletionCallback, Call(WGPUFenceCompletionStatus_DeviceLost, nullptr))
        .Times(1);
    ASSERT_DEVICE_ERROR(fence.OnCompletion(2u, ToMockFenceOnCompletionFails, nullptr));
    ASSERT_DEVICE_ERROR(device.Tick());

    // completed value is the last value signaled (all previous GPU operations are as if completed)
    EXPECT_EQ(fence.GetCompletedValue(), 2u);
}

// Test that Fence::OnCompletion callbacks with device lost status when device is lost after calling
// OnCompletion
TEST_P(DeviceLostTest, FenceOnCompletionBeforeLossFails) {
    wgpu::Fence fence;
    EXPECT_DEPRECATION_WARNING(fence = queue.CreateFence());

    queue.Signal(fence, 2);

    // callback should have device lost status
    EXPECT_CALL(*mockFenceOnCompletionCallback, Call(WGPUFenceCompletionStatus_DeviceLost, nullptr))
        .Times(1);
    fence.OnCompletion(2u, ToMockFenceOnCompletionFails, nullptr);
    SetCallbackAndLoseForTesting();
    ASSERT_DEVICE_ERROR(device.Tick());

    EXPECT_EQ(fence.GetCompletedValue(), 2u);
}

// Test that QueueOnSubmittedWorkDone fails after device is lost.
TEST_P(DeviceLostTest, QueueOnSubmittedWorkDoneFails) {
    SetCallbackAndLoseForTesting();

    // callback should have device lost status
    EXPECT_CALL(*mockQueueWorkDoneCallback, Call(WGPUQueueWorkDoneStatus_DeviceLost, nullptr))
        .Times(1);
    ASSERT_DEVICE_ERROR(queue.OnSubmittedWorkDone(0, ToMockQueueWorkDone, nullptr));
    ASSERT_DEVICE_ERROR(device.Tick());
}

// Test that QueueOnSubmittedWorkDone when the device is lost after calling OnSubmittedWorkDone
TEST_P(DeviceLostTest, QueueOnSubmittedWorkDoneBeforeLossFails) {
    // callback should have device lost status
    EXPECT_CALL(*mockQueueWorkDoneCallback, Call(WGPUQueueWorkDoneStatus_DeviceLost, nullptr))
        .Times(1);
    queue.OnSubmittedWorkDone(0, ToMockQueueWorkDone, nullptr);

    SetCallbackAndLoseForTesting();
    ASSERT_DEVICE_ERROR(device.Tick());
}

// Regression test for the Null backend not properly setting the completedSerial when
// WaitForIdleForDestruction is called, causing the fence signaling to not be retired and an
// ASSERT to fire.
TEST_P(DeviceLostTest, AfterSubmitAndSerial) {
    queue.Submit(0, nullptr);

    wgpu::Fence fence;
    EXPECT_DEPRECATION_WARNING(fence = queue.CreateFence());

    queue.Signal(fence, 1);
    SetCallbackAndLoseForTesting();
}

// Test that when you Signal, then Tick, then device lost, the fence completed value would be 2
TEST_P(DeviceLostTest, FenceSignalTickOnCompletion) {
    wgpu::Fence fence;
    EXPECT_DEPRECATION_WARNING(fence = queue.CreateFence());

    queue.Signal(fence, 2);
    WaitForAllOperations();

    // callback should have device lost status
    EXPECT_CALL(*mockFenceOnCompletionCallback, Call(WGPUFenceCompletionStatus_Success, nullptr))
        .Times(1);
    fence.OnCompletion(2u, ToMockFenceOnCompletionSucceeds, nullptr);
    SetCallbackAndLoseForTesting();

    EXPECT_EQ(fence.GetCompletedValue(), 2u);
}

// Test that LostForTesting can only be called on one time
TEST_P(DeviceLostTest, LoseForTestingOnce) {
    // First LoseForTesting call should occur normally
    SetCallbackAndLoseForTesting();

    // Second LoseForTesting call should result in no callbacks. The LoseForTesting will return
    // without doing anything when it sees that device has already been lost.
    device.SetDeviceLostCallback(ToMockDeviceLostCallback, this);
    EXPECT_CALL(*mockDeviceLostCallback, Call(_, this)).Times(0);
    device.LoseForTesting();
}

TEST_P(DeviceLostTest, DeviceLostDoesntCallUncapturedError) {
    // Set no callback.
    device.SetDeviceLostCallback(nullptr, nullptr);

    // Set the uncaptured error callback which should not be called on
    // device lost.
    MockCallback<WGPUErrorCallback> mockErrorCallback;
    device.SetUncapturedErrorCallback(mockErrorCallback.Callback(),
                                      mockErrorCallback.MakeUserdata(nullptr));
    EXPECT_CALL(mockErrorCallback, Call(_, _, _)).Times(Exactly(0));
    device.LoseForTesting();
}

// Test that WGPUCreatePipelineAsyncStatus_DeviceLost can be correctly returned when device is lost
// before the callback of Create*PipelineAsync() is called.
TEST_P(DeviceLostTest, DeviceLostBeforeCreatePipelineAsyncCallback) {
    wgpu::ShaderModule csModule = utils::CreateShaderModule(device, R"(
        [[stage(compute)]] fn main() {
        })");

    wgpu::ComputePipelineDescriptor descriptor;
    descriptor.computeStage.module = csModule;
    descriptor.computeStage.entryPoint = "main";

    auto callback = [](WGPUCreatePipelineAsyncStatus status, WGPUComputePipeline returnPipeline,
                       const char* message, void* userdata) {
        EXPECT_EQ(WGPUCreatePipelineAsyncStatus::WGPUCreatePipelineAsyncStatus_DeviceLost, status);
    };

    device.CreateComputePipelineAsync(&descriptor, callback, nullptr);
    SetCallbackAndLoseForTesting();
}

DAWN_INSTANTIATE_TEST(DeviceLostTest,
                      D3D12Backend(),
                      MetalBackend(),
                      NullBackend(),
                      OpenGLBackend(),
                      OpenGLESBackend(),
                      VulkanBackend());
