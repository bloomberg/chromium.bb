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

#include <vector>

#include "dawn/tests/DawnTest.h"
#include "dawn/utils/WGPUHelpers.h"

class ComputeStorageBufferBarrierTests : public DawnTest {
  protected:
    static constexpr uint32_t kNumValues = 100;
    static constexpr uint32_t kIterations = 100;
};

// Test that multiple dispatches to increment values in a storage buffer are synchronized.
TEST_P(ComputeStorageBufferBarrierTests, AddIncrement) {
    std::vector<uint32_t> data(kNumValues, 0);
    std::vector<uint32_t> expected(kNumValues, 0x1234 * kIterations);

    uint64_t bufferSize = static_cast<uint64_t>(data.size() * sizeof(uint32_t));
    wgpu::Buffer buffer = utils::CreateBufferFromData(
        device, data.data(), bufferSize, wgpu::BufferUsage::Storage | wgpu::BufferUsage::CopySrc);

    wgpu::ShaderModule module = utils::CreateShaderModule(device, R"(
        struct Buf {
            data : array<u32, 100>
        }

        @group(0) @binding(0) var<storage, read_write> buf : Buf;

        @compute @workgroup_size(1)
        fn main(@builtin(global_invocation_id) GlobalInvocationID : vec3<u32>) {
            buf.data[GlobalInvocationID.x] = buf.data[GlobalInvocationID.x] + 0x1234u;
        }
    )");

    wgpu::ComputePipelineDescriptor pipelineDesc = {};
    pipelineDesc.compute.module = module;
    pipelineDesc.compute.entryPoint = "main";
    wgpu::ComputePipeline pipeline = device.CreateComputePipeline(&pipelineDesc);

    wgpu::BindGroup bindGroup =
        utils::MakeBindGroup(device, pipeline.GetBindGroupLayout(0), {{0, buffer, 0, bufferSize}});

    wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
    wgpu::ComputePassEncoder pass = encoder.BeginComputePass();
    pass.SetPipeline(pipeline);
    pass.SetBindGroup(0, bindGroup);
    for (uint32_t i = 0; i < kIterations; ++i) {
        pass.DispatchWorkgroups(kNumValues);
    }
    pass.End();
    wgpu::CommandBuffer commands = encoder.Finish();
    queue.Submit(1, &commands);

    EXPECT_BUFFER_U32_RANGE_EQ(expected.data(), buffer, 0, kNumValues);
}

// Test that multiple dispatches to increment values by ping-ponging between two storage buffers
// are synchronized.
TEST_P(ComputeStorageBufferBarrierTests, AddPingPong) {
    std::vector<uint32_t> data(kNumValues, 0);
    std::vector<uint32_t> expectedA(kNumValues, 0x1234 * kIterations);
    std::vector<uint32_t> expectedB(kNumValues, 0x1234 * (kIterations - 1));

    uint64_t bufferSize = static_cast<uint64_t>(data.size() * sizeof(uint32_t));

    wgpu::Buffer bufferA = utils::CreateBufferFromData(
        device, data.data(), bufferSize, wgpu::BufferUsage::Storage | wgpu::BufferUsage::CopySrc);

    wgpu::Buffer bufferB = utils::CreateBufferFromData(
        device, data.data(), bufferSize, wgpu::BufferUsage::Storage | wgpu::BufferUsage::CopySrc);

    wgpu::ShaderModule module = utils::CreateShaderModule(device, R"(
        struct Buf {
            data : array<u32, 100>
        }

        @group(0) @binding(0) var<storage, read_write> src : Buf;
        @group(0) @binding(1) var<storage, read_write> dst : Buf;

        @compute @workgroup_size(1)
        fn main(@builtin(global_invocation_id) GlobalInvocationID : vec3<u32>) {
            dst.data[GlobalInvocationID.x] = src.data[GlobalInvocationID.x] + 0x1234u;
        }
    )");

    wgpu::ComputePipelineDescriptor pipelineDesc = {};
    pipelineDesc.compute.module = module;
    pipelineDesc.compute.entryPoint = "main";
    wgpu::ComputePipeline pipeline = device.CreateComputePipeline(&pipelineDesc);

    wgpu::BindGroup bindGroupA = utils::MakeBindGroup(device, pipeline.GetBindGroupLayout(0),
                                                      {
                                                          {0, bufferA, 0, bufferSize},
                                                          {1, bufferB, 0, bufferSize},
                                                      });

    wgpu::BindGroup bindGroupB = utils::MakeBindGroup(device, pipeline.GetBindGroupLayout(0),
                                                      {
                                                          {0, bufferB, 0, bufferSize},
                                                          {1, bufferA, 0, bufferSize},
                                                      });

    wgpu::BindGroup bindGroups[2] = {bindGroupA, bindGroupB};

    wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
    wgpu::ComputePassEncoder pass = encoder.BeginComputePass();
    pass.SetPipeline(pipeline);

    for (uint32_t i = 0; i < kIterations / 2; ++i) {
        pass.SetBindGroup(0, bindGroups[0]);
        pass.DispatchWorkgroups(kNumValues);
        pass.SetBindGroup(0, bindGroups[1]);
        pass.DispatchWorkgroups(kNumValues);
    }
    pass.End();
    wgpu::CommandBuffer commands = encoder.Finish();
    queue.Submit(1, &commands);

    EXPECT_BUFFER_U32_RANGE_EQ(expectedA.data(), bufferA, 0, kNumValues);
    EXPECT_BUFFER_U32_RANGE_EQ(expectedB.data(), bufferB, 0, kNumValues);
}

// Test that multiple dispatches to increment values by ping-ponging between storage buffers and
// read-only storage buffers are synchronized in one compute pass.
TEST_P(ComputeStorageBufferBarrierTests, StorageAndReadonlyStoragePingPongInOnePass) {
    std::vector<uint32_t> data(kNumValues, 0);
    std::vector<uint32_t> expectedA(kNumValues, 0x1234 * kIterations);
    std::vector<uint32_t> expectedB(kNumValues, 0x1234 * (kIterations - 1));

    uint64_t bufferSize = static_cast<uint64_t>(data.size() * sizeof(uint32_t));

    wgpu::Buffer bufferA = utils::CreateBufferFromData(
        device, data.data(), bufferSize, wgpu::BufferUsage::Storage | wgpu::BufferUsage::CopySrc);

    wgpu::Buffer bufferB = utils::CreateBufferFromData(
        device, data.data(), bufferSize, wgpu::BufferUsage::Storage | wgpu::BufferUsage::CopySrc);

    wgpu::ShaderModule module = utils::CreateShaderModule(device, R"(
        struct Buf {
            data : array<u32, 100>
        }

        @group(0) @binding(0) var<storage, read> src : Buf;
        @group(0) @binding(1) var<storage, read_write> dst : Buf;

        @compute @workgroup_size(1)
        fn main(@builtin(global_invocation_id) GlobalInvocationID : vec3<u32>) {
            dst.data[GlobalInvocationID.x] = src.data[GlobalInvocationID.x] + 0x1234u;
        }
    )");

    wgpu::ComputePipelineDescriptor pipelineDesc = {};
    pipelineDesc.compute.module = module;
    pipelineDesc.compute.entryPoint = "main";
    wgpu::ComputePipeline pipeline = device.CreateComputePipeline(&pipelineDesc);

    wgpu::BindGroup bindGroupA = utils::MakeBindGroup(device, pipeline.GetBindGroupLayout(0),
                                                      {
                                                          {0, bufferA, 0, bufferSize},
                                                          {1, bufferB, 0, bufferSize},
                                                      });

    wgpu::BindGroup bindGroupB = utils::MakeBindGroup(device, pipeline.GetBindGroupLayout(0),
                                                      {
                                                          {0, bufferB, 0, bufferSize},
                                                          {1, bufferA, 0, bufferSize},
                                                      });

    wgpu::BindGroup bindGroups[2] = {bindGroupA, bindGroupB};

    wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
    wgpu::ComputePassEncoder pass = encoder.BeginComputePass();
    pass.SetPipeline(pipeline);

    for (uint32_t i = 0; i < kIterations / 2; ++i) {
        pass.SetBindGroup(0, bindGroups[0]);
        pass.DispatchWorkgroups(kNumValues);
        pass.SetBindGroup(0, bindGroups[1]);
        pass.DispatchWorkgroups(kNumValues);
    }
    pass.End();
    wgpu::CommandBuffer commands = encoder.Finish();
    queue.Submit(1, &commands);

    EXPECT_BUFFER_U32_RANGE_EQ(expectedA.data(), bufferA, 0, kNumValues);
    EXPECT_BUFFER_U32_RANGE_EQ(expectedB.data(), bufferB, 0, kNumValues);
}

// Test that Storage to Uniform buffer transitions work and synchronize correctly
// by ping-ponging between Storage/Uniform usage in sequential compute passes.
TEST_P(ComputeStorageBufferBarrierTests, UniformToStorageAddPingPong) {
    std::vector<uint32_t> data(kNumValues, 0);
    std::vector<uint32_t> expectedA(kNumValues, 0x1234 * kIterations);
    std::vector<uint32_t> expectedB(kNumValues, 0x1234 * (kIterations - 1));

    uint64_t bufferSize = static_cast<uint64_t>(data.size() * sizeof(uint32_t));

    wgpu::Buffer bufferA = utils::CreateBufferFromData(
        device, data.data(), bufferSize,
        wgpu::BufferUsage::Storage | wgpu::BufferUsage::Uniform | wgpu::BufferUsage::CopySrc);

    wgpu::Buffer bufferB = utils::CreateBufferFromData(
        device, data.data(), bufferSize,
        wgpu::BufferUsage::Storage | wgpu::BufferUsage::Uniform | wgpu::BufferUsage::CopySrc);

    wgpu::ShaderModule module = utils::CreateShaderModule(device, R"(
        struct Buf {
            data : array<vec4<u32>, 25>
        }

        @group(0) @binding(0) var<uniform> src : Buf;
        @group(0) @binding(1) var<storage, read_write> dst : Buf;

        @compute @workgroup_size(1)
        fn main(@builtin(global_invocation_id) GlobalInvocationID : vec3<u32>) {
            dst.data[GlobalInvocationID.x] = src.data[GlobalInvocationID.x] +
                vec4<u32>(0x1234u, 0x1234u, 0x1234u, 0x1234u);
        }
    )");

    wgpu::ComputePipelineDescriptor pipelineDesc = {};
    pipelineDesc.compute.module = module;
    pipelineDesc.compute.entryPoint = "main";
    wgpu::ComputePipeline pipeline = device.CreateComputePipeline(&pipelineDesc);

    wgpu::BindGroup bindGroupA = utils::MakeBindGroup(device, pipeline.GetBindGroupLayout(0),
                                                      {
                                                          {0, bufferA, 0, bufferSize},
                                                          {1, bufferB, 0, bufferSize},
                                                      });

    wgpu::BindGroup bindGroupB = utils::MakeBindGroup(device, pipeline.GetBindGroupLayout(0),
                                                      {
                                                          {0, bufferB, 0, bufferSize},
                                                          {1, bufferA, 0, bufferSize},
                                                      });

    wgpu::BindGroup bindGroups[2] = {bindGroupA, bindGroupB};

    wgpu::CommandEncoder encoder = device.CreateCommandEncoder();

    for (uint32_t i = 0, b = 0; i < kIterations; ++i, b = 1 - b) {
        wgpu::ComputePassEncoder pass = encoder.BeginComputePass();
        pass.SetPipeline(pipeline);
        pass.SetBindGroup(0, bindGroups[b]);
        pass.DispatchWorkgroups(kNumValues / 4);
        pass.End();
    }

    wgpu::CommandBuffer commands = encoder.Finish();
    queue.Submit(1, &commands);

    EXPECT_BUFFER_U32_RANGE_EQ(expectedA.data(), bufferA, 0, kNumValues);
    EXPECT_BUFFER_U32_RANGE_EQ(expectedB.data(), bufferB, 0, kNumValues);
}

// Test that Storage to Uniform buffer transitions work and synchronize correctly
// by ping-ponging between Storage/Uniform usage in one compute pass.
TEST_P(ComputeStorageBufferBarrierTests, UniformToStorageAddPingPongInOnePass) {
    std::vector<uint32_t> data(kNumValues, 0);
    std::vector<uint32_t> expectedA(kNumValues, 0x1234 * kIterations);
    std::vector<uint32_t> expectedB(kNumValues, 0x1234 * (kIterations - 1));

    uint64_t bufferSize = static_cast<uint64_t>(data.size() * sizeof(uint32_t));

    wgpu::Buffer bufferA = utils::CreateBufferFromData(
        device, data.data(), bufferSize,
        wgpu::BufferUsage::Storage | wgpu::BufferUsage::Uniform | wgpu::BufferUsage::CopySrc);

    wgpu::Buffer bufferB = utils::CreateBufferFromData(
        device, data.data(), bufferSize,
        wgpu::BufferUsage::Storage | wgpu::BufferUsage::Uniform | wgpu::BufferUsage::CopySrc);

    wgpu::ShaderModule module = utils::CreateShaderModule(device, R"(
        struct Buf {
            data : array<vec4<u32>, 25>
        }

        @group(0) @binding(0) var<uniform> src : Buf;
        @group(0) @binding(1) var<storage, read_write> dst : Buf;

        @compute @workgroup_size(1)
        fn main(@builtin(global_invocation_id) GlobalInvocationID : vec3<u32>) {
            dst.data[GlobalInvocationID.x] = src.data[GlobalInvocationID.x] +
                vec4<u32>(0x1234u, 0x1234u, 0x1234u, 0x1234u);
        }
    )");

    wgpu::ComputePipelineDescriptor pipelineDesc = {};
    pipelineDesc.compute.module = module;
    pipelineDesc.compute.entryPoint = "main";
    wgpu::ComputePipeline pipeline = device.CreateComputePipeline(&pipelineDesc);

    wgpu::BindGroup bindGroupA = utils::MakeBindGroup(device, pipeline.GetBindGroupLayout(0),
                                                      {
                                                          {0, bufferA, 0, bufferSize},
                                                          {1, bufferB, 0, bufferSize},
                                                      });

    wgpu::BindGroup bindGroupB = utils::MakeBindGroup(device, pipeline.GetBindGroupLayout(0),
                                                      {
                                                          {0, bufferB, 0, bufferSize},
                                                          {1, bufferA, 0, bufferSize},
                                                      });

    wgpu::BindGroup bindGroups[2] = {bindGroupA, bindGroupB};

    wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
    wgpu::ComputePassEncoder pass = encoder.BeginComputePass();
    for (uint32_t i = 0, b = 0; i < kIterations; ++i, b = 1 - b) {
        pass.SetPipeline(pipeline);
        pass.SetBindGroup(0, bindGroups[b]);
        pass.DispatchWorkgroups(kNumValues / 4);
    }
    pass.End();

    wgpu::CommandBuffer commands = encoder.Finish();
    queue.Submit(1, &commands);

    EXPECT_BUFFER_U32_RANGE_EQ(expectedA.data(), bufferA, 0, kNumValues);
    EXPECT_BUFFER_U32_RANGE_EQ(expectedB.data(), bufferB, 0, kNumValues);
}

// Test that barriers for dispatches correctly combine Indirect | Storage in backends with explicit
// barriers. Do this by:
//  1 - Initializing an indirect buffer with zeros.
//  2 - Write ones into it with a compute shader.
//  3 - Use the indirect buffer in a Dispatch while also reading its data.
TEST_P(ComputeStorageBufferBarrierTests, IndirectBufferCorrectBarrier) {
    wgpu::ComputePipelineDescriptor step2PipelineDesc;
    step2PipelineDesc.compute.entryPoint = "main";
    step2PipelineDesc.compute.module = utils::CreateShaderModule(device, R"(
        struct Buf {
            data : array<u32, 3>
        }
        @group(0) @binding(0) var<storage, read_write> buf : Buf;

        @compute @workgroup_size(1) fn main() {
            buf.data = array<u32, 3>(1u, 1u, 1u);
        }
    )");
    wgpu::ComputePipeline step2Pipeline = device.CreateComputePipeline(&step2PipelineDesc);

    wgpu::ComputePipelineDescriptor step3PipelineDesc;
    step3PipelineDesc.compute.entryPoint = "main";
    step3PipelineDesc.compute.module = utils::CreateShaderModule(device, R"(
        struct Buf {
            data : array<u32, 3>
        }
        @group(0) @binding(0) var<storage, read> buf : Buf;

        struct Result {
            data : u32
        }
        @group(0) @binding(1) var<storage, read_write> result : Result;

        @compute @workgroup_size(1) fn main() {
            result.data = 2u;
            if (buf.data[0] == 1u && buf.data[1] == 1u && buf.data[2] == 1u) {
                result.data = 1u;
            }
        }
    )");
    wgpu::ComputePipeline step3Pipeline = device.CreateComputePipeline(&step3PipelineDesc);

    //  1 - Initializing an indirect buffer with zeros.
    wgpu::Buffer buf = utils::CreateBufferFromData<uint32_t>(
        device, wgpu::BufferUsage::Storage | wgpu::BufferUsage::Indirect, {0u, 0u, 0u});

    //  2 - Write ones into it with a compute shader.
    wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
    wgpu::ComputePassEncoder pass = encoder.BeginComputePass();

    wgpu::BindGroup step2Group =
        utils::MakeBindGroup(device, step2Pipeline.GetBindGroupLayout(0), {{0, buf}});

    pass.SetPipeline(step2Pipeline);
    pass.SetBindGroup(0, step2Group);
    pass.DispatchWorkgroups(1);

    //  3 - Use the indirect buffer in a Dispatch while also reading its data.
    wgpu::Buffer resultBuffer = utils::CreateBufferFromData<uint32_t>(
        device, wgpu::BufferUsage::Storage | wgpu::BufferUsage::CopySrc, {0u});
    wgpu::BindGroup step3Group = utils::MakeBindGroup(device, step3Pipeline.GetBindGroupLayout(0),
                                                      {{0, buf}, {1, resultBuffer}});

    pass.SetPipeline(step3Pipeline);
    pass.SetBindGroup(0, step3Group);
    pass.DispatchWorkgroupsIndirect(buf, 0);

    pass.End();
    wgpu::CommandBuffer commands = encoder.Finish();
    queue.Submit(1, &commands);

    EXPECT_BUFFER_U32_EQ(1u, resultBuffer, 0);
}

DAWN_INSTANTIATE_TEST(ComputeStorageBufferBarrierTests,
                      D3D12Backend(),
                      MetalBackend(),
                      OpenGLBackend(),
                      OpenGLESBackend(),
                      VulkanBackend());
