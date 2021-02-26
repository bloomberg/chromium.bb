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

#include "tests/unittests/validation/ValidationTest.h"

#include "utils/ComboRenderBundleEncoderDescriptor.h"
#include "utils/ComboRenderPipelineDescriptor.h"
#include "utils/WGPUHelpers.h"

class UnsafeAPIValidationTest : public ValidationTest {
  protected:
    wgpu::Device CreateTestDevice() override {
        dawn_native::DeviceDescriptor descriptor;
        descriptor.forceEnabledToggles.push_back("disallow_unsafe_apis");
        return wgpu::Device::Acquire(adapter.CreateDevice(&descriptor));
    }
};

// Check that DrawIndexedIndirect is disallowed as part of unsafe APIs.
TEST_F(UnsafeAPIValidationTest, DrawIndexedIndirectDisallowed) {
    // Create the index and indirect buffers.
    wgpu::BufferDescriptor indexBufferDesc;
    indexBufferDesc.size = 4;
    indexBufferDesc.usage = wgpu::BufferUsage::Index;
    wgpu::Buffer indexBuffer = device.CreateBuffer(&indexBufferDesc);

    wgpu::BufferDescriptor indirectBufferDesc;
    indirectBufferDesc.size = 64;
    indirectBufferDesc.usage = wgpu::BufferUsage::Indirect;
    wgpu::Buffer indirectBuffer = device.CreateBuffer(&indirectBufferDesc);

    // The RenderPassDescriptor, RenderBundleDescriptor and pipeline for all sub-tests below.
    DummyRenderPass renderPass(device);

    utils::ComboRenderBundleEncoderDescriptor bundleDesc = {};
    bundleDesc.colorFormatsCount = 1;
    bundleDesc.cColorFormats[0] = renderPass.attachmentFormat;

    utils::ComboRenderPipelineDescriptor desc(device);
    desc.vertexStage.module = utils::CreateShaderModule(device, utils::SingleShaderStage::Vertex,
                                                        "#version 450\nvoid main() {}");
    desc.cFragmentStage.module = utils::CreateShaderModule(
        device, utils::SingleShaderStage::Fragment, "#version 450\nvoid main() {}");
    wgpu::RenderPipeline pipeline = device.CreateRenderPipeline(&desc);

    // Control cases: DrawIndirect and DrawIndexed are allowed inside a render pass.
    {
        wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
        wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&renderPass);
        pass.SetPipeline(pipeline);

        pass.SetIndexBufferWithFormat(indexBuffer, wgpu::IndexFormat::Uint32);
        pass.DrawIndexed(1);

        pass.DrawIndirect(indirectBuffer, 0);
        pass.EndPass();
        encoder.Finish();
    }

    // Control case: DrawIndirect and DrawIndexed are allowed inside a render bundle.
    {
        wgpu::RenderBundleEncoder encoder = device.CreateRenderBundleEncoder(&bundleDesc);
        encoder.SetPipeline(pipeline);

        encoder.SetIndexBufferWithFormat(indexBuffer, wgpu::IndexFormat::Uint32);
        encoder.DrawIndexed(1);

        encoder.DrawIndirect(indirectBuffer, 0);
        encoder.Finish();
    }

    // Error case, DrawIndexedIndirect is disallowed inside a render pass.
    {
        wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
        wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&renderPass);

        pass.SetPipeline(pipeline);
        pass.SetIndexBufferWithFormat(indexBuffer, wgpu::IndexFormat::Uint32);
        pass.DrawIndexedIndirect(indirectBuffer, 0);

        pass.EndPass();
        ASSERT_DEVICE_ERROR(encoder.Finish());
    }

    // Error case, DrawIndexedIndirect is disallowed inside a render bundle.
    {
        wgpu::RenderBundleEncoder encoder = device.CreateRenderBundleEncoder(&bundleDesc);

        encoder.SetPipeline(pipeline);
        encoder.SetIndexBufferWithFormat(indexBuffer, wgpu::IndexFormat::Uint32);
        encoder.DrawIndexedIndirect(indirectBuffer, 0);

        ASSERT_DEVICE_ERROR(encoder.Finish());
    }
}

// Check that DispatchIndirect is disallowed as part of unsafe APIs.
TEST_F(UnsafeAPIValidationTest, DispatchIndirectDisallowed) {
    // Create the index and indirect buffers.
    wgpu::BufferDescriptor indirectBufferDesc;
    indirectBufferDesc.size = 64;
    indirectBufferDesc.usage = wgpu::BufferUsage::Indirect;
    wgpu::Buffer indirectBuffer = device.CreateBuffer(&indirectBufferDesc);

    // Create the dummy compute pipeline.
    wgpu::ComputePipelineDescriptor pipelineDesc;
    pipelineDesc.computeStage.entryPoint = "main";
    pipelineDesc.computeStage.module = utils::CreateShaderModule(
        device, utils::SingleShaderStage::Compute, "#version 450\nvoid main() {}");
    wgpu::ComputePipeline pipeline = device.CreateComputePipeline(&pipelineDesc);

    // Control case: dispatch is allowed.
    {
        wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
        wgpu::ComputePassEncoder pass = encoder.BeginComputePass();

        pass.SetPipeline(pipeline);
        pass.Dispatch(1, 1, 1);

        pass.EndPass();
        encoder.Finish();
    }

    // Error case: dispatch indirect is disallowed.
    {
        wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
        wgpu::ComputePassEncoder pass = encoder.BeginComputePass();

        pass.SetPipeline(pipeline);
        pass.DispatchIndirect(indirectBuffer, 0);

        pass.EndPass();
        ASSERT_DEVICE_ERROR(encoder.Finish());
    }
}

// Check that dynamic storage buffers are disallowed.
TEST_F(UnsafeAPIValidationTest, DynamicStorageBuffer) {
    wgpu::BindGroupLayoutEntry entry;
    entry.type = wgpu::BindingType::StorageBuffer;
    entry.visibility = wgpu::ShaderStage::Fragment;

    wgpu::BindGroupLayoutDescriptor desc;
    desc.entries = &entry;
    desc.entryCount = 1;

    // Control case: storage buffer without a dynamic offset is allowed.
    {
        entry.hasDynamicOffset = false;
        device.CreateBindGroupLayout(&desc);
    }

    // Control case: storage buffer with a dynamic offset is disallowed.
    {
        entry.hasDynamicOffset = true;
        ASSERT_DEVICE_ERROR(device.CreateBindGroupLayout(&desc));
    }
}
