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

#include "utils/WGPUHelpers.h"

#include <initializer_list>

constexpr static std::initializer_list<uint32_t> kSentinelData{0, 0, 0};

class ComputeDispatchTests : public DawnTest {
  protected:
    void SetUp() override {
        DawnTest::SetUp();

        // Write workgroup number into the output buffer if we saw the biggest dispatch
        // To make sure the dispatch was not called, write maximum u32 value for 0 dispatches
        wgpu::ShaderModule moduleForDispatch = utils::CreateShaderModule(device, R"(
            [[block]] struct OutputBuf {
                workGroups : vec3<u32>;
            };

            [[group(0), binding(0)]] var<storage, read_write> output : OutputBuf;

            [[stage(compute), workgroup_size(1, 1, 1)]]
            fn main([[builtin(global_invocation_id)]] GlobalInvocationID : vec3<u32>,
                    [[builtin(num_workgroups)]] dispatch : vec3<u32>) {
                if (dispatch.x == 0u || dispatch.y == 0u || dispatch.z == 0u) {
                    output.workGroups = vec3<u32>(0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu);
                    return;
                }

                if (all(GlobalInvocationID == dispatch - vec3<u32>(1u, 1u, 1u))) {
                    output.workGroups = dispatch;
                }
            })");

        // TODO(dawn:839): use moduleForDispatch for indirect dispatch tests when D3D12 supports
        // [[num_workgroups]] for indirect dispatch.
        wgpu::ShaderModule moduleForDispatchIndirect = utils::CreateShaderModule(device, R"(
            [[block]] struct InputBuf {
                expectedDispatch : vec3<u32>;
            };
            [[block]] struct OutputBuf {
                workGroups : vec3<u32>;
            };

            [[group(0), binding(0)]] var<uniform> input : InputBuf;
            [[group(0), binding(1)]] var<storage, read_write> output : OutputBuf;

            [[stage(compute), workgroup_size(1, 1, 1)]]
            fn main([[builtin(global_invocation_id)]] GlobalInvocationID : vec3<u32>) {
                let dispatch : vec3<u32> = input.expectedDispatch;

                if (dispatch.x == 0u || dispatch.y == 0u || dispatch.z == 0u) {
                    output.workGroups = vec3<u32>(0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu);
                    return;
                }

                if (all(GlobalInvocationID == dispatch - vec3<u32>(1u, 1u, 1u))) {
                    output.workGroups = dispatch;
                }
            })");

        wgpu::ComputePipelineDescriptor csDesc;
        csDesc.compute.module = moduleForDispatch;
        csDesc.compute.entryPoint = "main";
        pipelineForDispatch = device.CreateComputePipeline(&csDesc);

        csDesc.compute.module = moduleForDispatchIndirect;
        pipelineForDispatchIndirect = device.CreateComputePipeline(&csDesc);
    }

    void DirectTest(uint32_t x, uint32_t y, uint32_t z) {
        // Set up dst storage buffer to contain dispatch x, y, z
        wgpu::Buffer dst = utils::CreateBufferFromData<uint32_t>(
            device,
            wgpu::BufferUsage::Storage | wgpu::BufferUsage::CopySrc | wgpu::BufferUsage::CopyDst,
            kSentinelData);

        // Set up bind group and issue dispatch
        wgpu::BindGroup bindGroup =
            utils::MakeBindGroup(device, pipelineForDispatch.GetBindGroupLayout(0),
                                 {
                                     {0, dst, 0, 3 * sizeof(uint32_t)},
                                 });

        wgpu::CommandBuffer commands;
        {
            wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
            wgpu::ComputePassEncoder pass = encoder.BeginComputePass();
            pass.SetPipeline(pipelineForDispatch);
            pass.SetBindGroup(0, bindGroup);
            pass.Dispatch(x, y, z);
            pass.EndPass();

            commands = encoder.Finish();
        }

        queue.Submit(1, &commands);

        std::vector<uint32_t> expected =
            x == 0 || y == 0 || z == 0 ? kSentinelData : std::initializer_list<uint32_t>{x, y, z};

        // Verify the dispatch got called if all group counts are not zero
        EXPECT_BUFFER_U32_RANGE_EQ(&expected[0], dst, 0, 3);
    }

    void IndirectTest(std::vector<uint32_t> indirectBufferData, uint64_t indirectOffset) {
        // Set up dst storage buffer to contain dispatch x, y, z
        wgpu::Buffer dst = utils::CreateBufferFromData<uint32_t>(
            device,
            wgpu::BufferUsage::Storage | wgpu::BufferUsage::CopySrc | wgpu::BufferUsage::CopyDst,
            kSentinelData);

        wgpu::Buffer indirectBuffer = utils::CreateBufferFromData(
            device, &indirectBufferData[0], indirectBufferData.size() * sizeof(uint32_t),
            wgpu::BufferUsage::Indirect);

        uint32_t indirectStart = indirectOffset / sizeof(uint32_t);

        wgpu::Buffer expectedBuffer =
            utils::CreateBufferFromData(device, &indirectBufferData[indirectStart],
                                        3 * sizeof(uint32_t), wgpu::BufferUsage::Uniform);

        // Set up bind group and issue dispatch
        wgpu::BindGroup bindGroup =
            utils::MakeBindGroup(device, pipelineForDispatchIndirect.GetBindGroupLayout(0),
                                 {
                                     {0, expectedBuffer, 0, 3 * sizeof(uint32_t)},
                                     {1, dst, 0, 3 * sizeof(uint32_t)},
                                 });

        wgpu::CommandBuffer commands;
        {
            wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
            wgpu::ComputePassEncoder pass = encoder.BeginComputePass();
            pass.SetPipeline(pipelineForDispatchIndirect);
            pass.SetBindGroup(0, bindGroup);
            pass.DispatchIndirect(indirectBuffer, indirectOffset);
            pass.EndPass();

            commands = encoder.Finish();
        }

        queue.Submit(1, &commands);

        std::vector<uint32_t> expected;

        uint32_t maxComputeWorkgroupsPerDimension =
            GetSupportedLimits().limits.maxComputeWorkgroupsPerDimension;
        if (indirectBufferData[indirectStart] == 0 || indirectBufferData[indirectStart + 1] == 0 ||
            indirectBufferData[indirectStart + 2] == 0 ||
            indirectBufferData[indirectStart] > maxComputeWorkgroupsPerDimension ||
            indirectBufferData[indirectStart + 1] > maxComputeWorkgroupsPerDimension ||
            indirectBufferData[indirectStart + 2] > maxComputeWorkgroupsPerDimension) {
            expected = kSentinelData;
        } else {
            expected.assign(indirectBufferData.begin() + indirectStart,
                            indirectBufferData.begin() + indirectStart + 3);
        }

        // Verify the dispatch got called with group counts in indirect buffer if all group counts
        // are not zero
        EXPECT_BUFFER_U32_RANGE_EQ(&expected[0], dst, 0, 3);
    }

  private:
    wgpu::ComputePipeline pipelineForDispatch;
    wgpu::ComputePipeline pipelineForDispatchIndirect;
};

// Test basic direct
TEST_P(ComputeDispatchTests, DirectBasic) {
    DirectTest(2, 3, 4);
}

// Test no-op direct
TEST_P(ComputeDispatchTests, DirectNoop) {
    // All dimensions are 0s
    DirectTest(0, 0, 0);

    // Only x dimension is 0
    DirectTest(0, 3, 4);

    // Only y dimension is 0
    DirectTest(2, 0, 4);

    // Only z dimension is 0
    DirectTest(2, 3, 0);
}

// Test basic indirect
TEST_P(ComputeDispatchTests, IndirectBasic) {
    IndirectTest({2, 3, 4}, 0);
}

// Test no-op indirect
TEST_P(ComputeDispatchTests, IndirectNoop) {
    // All dimensions are 0s
    IndirectTest({0, 0, 0}, 0);

    // Only x dimension is 0
    IndirectTest({0, 3, 4}, 0);

    // Only y dimension is 0
    IndirectTest({2, 0, 4}, 0);

    // Only z dimension is 0
    IndirectTest({2, 3, 0}, 0);
}

// Test indirect with buffer offset
TEST_P(ComputeDispatchTests, IndirectOffset) {
    IndirectTest({0, 0, 0, 2, 3, 4}, 3 * sizeof(uint32_t));
}

// Test indirect dispatches at max limit.
TEST_P(ComputeDispatchTests, MaxWorkgroups) {
    // TODO(crbug.com/dawn/1165): Fails with WARP
    DAWN_SUPPRESS_TEST_IF(IsWARP());
    uint32_t max = GetSupportedLimits().limits.maxComputeWorkgroupsPerDimension;

    // Test that the maximum works in each dimension.
    // Note: Testing (max, max, max) is very slow.
    IndirectTest({max, 3, 4}, 0);
    IndirectTest({2, max, 4}, 0);
    IndirectTest({2, 3, max}, 0);
}

// Test indirect dispatches exceeding the max limit are noop-ed.
TEST_P(ComputeDispatchTests, ExceedsMaxWorkgroupsNoop) {
    DAWN_TEST_UNSUPPORTED_IF(HasToggleEnabled("skip_validation"));

    uint32_t max = GetSupportedLimits().limits.maxComputeWorkgroupsPerDimension;

    // All dimensions are above the max
    IndirectTest({max + 1, max + 1, max + 1}, 0);

    // Only x dimension is above the max
    IndirectTest({max + 1, 3, 4}, 0);
    IndirectTest({2 * max, 3, 4}, 0);

    // Only y dimension is above the max
    IndirectTest({2, max + 1, 4}, 0);
    IndirectTest({2, 2 * max, 4}, 0);

    // Only z dimension is above the max
    IndirectTest({2, 3, max + 1}, 0);
    IndirectTest({2, 3, 2 * max}, 0);
}

// Test indirect dispatches exceeding the max limit with an offset are noop-ed.
TEST_P(ComputeDispatchTests, ExceedsMaxWorkgroupsWithOffsetNoop) {
    DAWN_TEST_UNSUPPORTED_IF(HasToggleEnabled("skip_validation"));

    uint32_t max = GetSupportedLimits().limits.maxComputeWorkgroupsPerDimension;

    IndirectTest({1, 2, 3, max + 1, 4, 5}, 1 * sizeof(uint32_t));
    IndirectTest({1, 2, 3, max + 1, 4, 5}, 2 * sizeof(uint32_t));
    IndirectTest({1, 2, 3, max + 1, 4, 5}, 3 * sizeof(uint32_t));
}

DAWN_INSTANTIATE_TEST(ComputeDispatchTests,
                      D3D12Backend(),
                      MetalBackend(),
                      OpenGLBackend(),
                      OpenGLESBackend(),
                      VulkanBackend());
