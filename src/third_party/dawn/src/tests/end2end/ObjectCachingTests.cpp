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

class ObjectCachingTest : public DawnTest {
  protected:
    void SetUp() override {
        DawnTest::SetUp();

        // TODO(crbug.com/tint/688): error: undeclared identifier '_tint_7'
        DAWN_SKIP_TEST_IF(IsD3D12() && HasToggleEnabled("use_tint_generator"));
    }
};

// Test that BindGroupLayouts are correctly deduplicated.
TEST_P(ObjectCachingTest, BindGroupLayoutDeduplication) {
    wgpu::BindGroupLayout bgl = utils::MakeBindGroupLayout(
        device, {{1, wgpu::ShaderStage::Fragment, wgpu::BufferBindingType::Uniform}});
    wgpu::BindGroupLayout sameBgl = utils::MakeBindGroupLayout(
        device, {{1, wgpu::ShaderStage::Fragment, wgpu::BufferBindingType::Uniform}});
    wgpu::BindGroupLayout otherBgl = utils::MakeBindGroupLayout(
        device, {{1, wgpu::ShaderStage::Vertex, wgpu::BufferBindingType::Uniform}});

    EXPECT_NE(bgl.Get(), otherBgl.Get());
    EXPECT_EQ(bgl.Get() == sameBgl.Get(), !UsesWire());
}

// Test that two similar bind group layouts won't refer to the same one if they differ by dynamic.
TEST_P(ObjectCachingTest, BindGroupLayoutDynamic) {
    wgpu::BindGroupLayout bgl = utils::MakeBindGroupLayout(
        device, {{1, wgpu::ShaderStage::Fragment, wgpu::BufferBindingType::Uniform, true}});
    wgpu::BindGroupLayout sameBgl = utils::MakeBindGroupLayout(
        device, {{1, wgpu::ShaderStage::Fragment, wgpu::BufferBindingType::Uniform, true}});
    wgpu::BindGroupLayout otherBgl = utils::MakeBindGroupLayout(
        device, {{1, wgpu::ShaderStage::Fragment, wgpu::BufferBindingType::Uniform, false}});

    EXPECT_NE(bgl.Get(), otherBgl.Get());
    EXPECT_EQ(bgl.Get() == sameBgl.Get(), !UsesWire());
}

// Test that two similar bind group layouts won't refer to the same one if they differ by
// textureComponentType
TEST_P(ObjectCachingTest, BindGroupLayoutTextureComponentType) {
    wgpu::BindGroupLayout bgl = utils::MakeBindGroupLayout(
        device, {{1, wgpu::ShaderStage::Fragment, wgpu::TextureSampleType::Float}});
    wgpu::BindGroupLayout sameBgl = utils::MakeBindGroupLayout(
        device, {{1, wgpu::ShaderStage::Fragment, wgpu::TextureSampleType::Float}});
    wgpu::BindGroupLayout otherBgl = utils::MakeBindGroupLayout(
        device, {{1, wgpu::ShaderStage::Fragment, wgpu::TextureSampleType::Uint}});

    EXPECT_NE(bgl.Get(), otherBgl.Get());
    EXPECT_EQ(bgl.Get() == sameBgl.Get(), !UsesWire());
}

// Test that two similar bind group layouts won't refer to the same one if they differ by
// viewDimension
TEST_P(ObjectCachingTest, BindGroupLayoutViewDimension) {
    wgpu::BindGroupLayout bgl = utils::MakeBindGroupLayout(
        device, {{1, wgpu::ShaderStage::Fragment, wgpu::TextureSampleType::Float}});
    wgpu::BindGroupLayout sameBgl = utils::MakeBindGroupLayout(
        device, {{1, wgpu::ShaderStage::Fragment, wgpu::TextureSampleType::Float}});
    wgpu::BindGroupLayout otherBgl = utils::MakeBindGroupLayout(
        device, {{1, wgpu::ShaderStage::Fragment, wgpu::TextureSampleType::Float,
                  wgpu::TextureViewDimension::e2DArray}});

    EXPECT_NE(bgl.Get(), otherBgl.Get());
    EXPECT_EQ(bgl.Get() == sameBgl.Get(), !UsesWire());
}

// Test that an error object doesn't try to uncache itself
TEST_P(ObjectCachingTest, ErrorObjectDoesntUncache) {
    DAWN_SKIP_TEST_IF(HasToggleEnabled("skip_validation"));

    ASSERT_DEVICE_ERROR(
        wgpu::BindGroupLayout bgl = utils::MakeBindGroupLayout(
            device, {{0, wgpu::ShaderStage::Fragment, wgpu::BufferBindingType::Uniform},
                     {0, wgpu::ShaderStage::Fragment, wgpu::BufferBindingType::Uniform}}));
}

// Test that PipelineLayouts are correctly deduplicated.
TEST_P(ObjectCachingTest, PipelineLayoutDeduplication) {
    wgpu::BindGroupLayout bgl = utils::MakeBindGroupLayout(
        device, {{1, wgpu::ShaderStage::Fragment, wgpu::BufferBindingType::Uniform}});
    wgpu::BindGroupLayout otherBgl = utils::MakeBindGroupLayout(
        device, {{1, wgpu::ShaderStage::Vertex, wgpu::BufferBindingType::Uniform}});

    wgpu::PipelineLayout pl = utils::MakeBasicPipelineLayout(device, &bgl);
    wgpu::PipelineLayout samePl = utils::MakeBasicPipelineLayout(device, &bgl);
    wgpu::PipelineLayout otherPl1 = utils::MakeBasicPipelineLayout(device, nullptr);
    wgpu::PipelineLayout otherPl2 = utils::MakeBasicPipelineLayout(device, &otherBgl);

    EXPECT_NE(pl.Get(), otherPl1.Get());
    EXPECT_NE(pl.Get(), otherPl2.Get());
    EXPECT_EQ(pl.Get() == samePl.Get(), !UsesWire());
}

// Test that ShaderModules are correctly deduplicated.
TEST_P(ObjectCachingTest, ShaderModuleDeduplication) {
    wgpu::ShaderModule module = utils::CreateShaderModule(device, R"(
        [[location(0)]] var<out> fragColor : vec4<f32>;
        [[stage(fragment)]] fn main() -> void {
            fragColor = vec4<f32>(0.0, 1.0, 0.0, 1.0);
        })");
    wgpu::ShaderModule sameModule = utils::CreateShaderModule(device, R"(
        [[location(0)]] var<out> fragColor : vec4<f32>;
        [[stage(fragment)]] fn main() -> void {
            fragColor = vec4<f32>(0.0, 1.0, 0.0, 1.0);
        })");
    wgpu::ShaderModule otherModule = utils::CreateShaderModule(device, R"(
        [[location(0)]] var<out> fragColor : vec4<f32>;
        [[stage(fragment)]] fn main() -> void {
            fragColor = vec4<f32>(0.0, 0.0, 0.0, 0.0);
        })");

    EXPECT_NE(module.Get(), otherModule.Get());
    EXPECT_EQ(module.Get() == sameModule.Get(), !UsesWire());
}

// Test that ComputePipeline are correctly deduplicated wrt. their ShaderModule
TEST_P(ObjectCachingTest, ComputePipelineDeduplicationOnShaderModule) {
    wgpu::ShaderModule module = utils::CreateShaderModule(device, R"(
        var<workgroup> i : u32;
        [[stage(compute)]] fn main() -> void {
            i = 0u;
        })");
    wgpu::ShaderModule sameModule = utils::CreateShaderModule(device, R"(
        var<workgroup> i : u32;
        [[stage(compute)]] fn main() -> void {
            i = 0u;
        })");
    wgpu::ShaderModule otherModule = utils::CreateShaderModule(device, R"(
        [[stage(compute)]] fn main() -> void {
        })");

    EXPECT_NE(module.Get(), otherModule.Get());
    EXPECT_EQ(module.Get() == sameModule.Get(), !UsesWire());

    wgpu::PipelineLayout layout = utils::MakeBasicPipelineLayout(device, nullptr);

    wgpu::ComputePipelineDescriptor desc;
    desc.computeStage.entryPoint = "main";
    desc.layout = layout;

    desc.computeStage.module = module;
    wgpu::ComputePipeline pipeline = device.CreateComputePipeline(&desc);

    desc.computeStage.module = sameModule;
    wgpu::ComputePipeline samePipeline = device.CreateComputePipeline(&desc);

    desc.computeStage.module = otherModule;
    wgpu::ComputePipeline otherPipeline = device.CreateComputePipeline(&desc);

    EXPECT_NE(pipeline.Get(), otherPipeline.Get());
    EXPECT_EQ(pipeline.Get() == samePipeline.Get(), !UsesWire());
}

// Test that ComputePipeline are correctly deduplicated wrt. their layout
TEST_P(ObjectCachingTest, ComputePipelineDeduplicationOnLayout) {
    wgpu::BindGroupLayout bgl = utils::MakeBindGroupLayout(
        device, {{1, wgpu::ShaderStage::Fragment, wgpu::BufferBindingType::Uniform}});
    wgpu::BindGroupLayout otherBgl = utils::MakeBindGroupLayout(
        device, {{1, wgpu::ShaderStage::Vertex, wgpu::BufferBindingType::Uniform}});

    wgpu::PipelineLayout pl = utils::MakeBasicPipelineLayout(device, &bgl);
    wgpu::PipelineLayout samePl = utils::MakeBasicPipelineLayout(device, &bgl);
    wgpu::PipelineLayout otherPl = utils::MakeBasicPipelineLayout(device, nullptr);

    EXPECT_NE(pl.Get(), otherPl.Get());
    EXPECT_EQ(pl.Get() == samePl.Get(), !UsesWire());

    wgpu::ComputePipelineDescriptor desc;
    desc.computeStage.entryPoint = "main";
    desc.computeStage.module = utils::CreateShaderModule(device, R"(
            var<workgroup> i : u32;
            [[stage(compute)]] fn main() -> void {
                i = 0u;
            })");

    desc.layout = pl;
    wgpu::ComputePipeline pipeline = device.CreateComputePipeline(&desc);

    desc.layout = samePl;
    wgpu::ComputePipeline samePipeline = device.CreateComputePipeline(&desc);

    desc.layout = otherPl;
    wgpu::ComputePipeline otherPipeline = device.CreateComputePipeline(&desc);

    EXPECT_NE(pipeline.Get(), otherPipeline.Get());
    EXPECT_EQ(pipeline.Get() == samePipeline.Get(), !UsesWire());
}

// Test that RenderPipelines are correctly deduplicated wrt. their layout
TEST_P(ObjectCachingTest, RenderPipelineDeduplicationOnLayout) {
    wgpu::BindGroupLayout bgl = utils::MakeBindGroupLayout(
        device, {{1, wgpu::ShaderStage::Fragment, wgpu::BufferBindingType::Uniform}});
    wgpu::BindGroupLayout otherBgl = utils::MakeBindGroupLayout(
        device, {{1, wgpu::ShaderStage::Vertex, wgpu::BufferBindingType::Uniform}});

    wgpu::PipelineLayout pl = utils::MakeBasicPipelineLayout(device, &bgl);
    wgpu::PipelineLayout samePl = utils::MakeBasicPipelineLayout(device, &bgl);
    wgpu::PipelineLayout otherPl = utils::MakeBasicPipelineLayout(device, nullptr);

    EXPECT_NE(pl.Get(), otherPl.Get());
    EXPECT_EQ(pl.Get() == samePl.Get(), !UsesWire());

    utils::ComboRenderPipelineDescriptor2 desc;
    desc.vertex.module = utils::CreateShaderModule(device, R"(
        [[builtin(position)]] var<out> Position : vec4<f32>;
        [[stage(vertex)]] fn main() -> void {
            Position = vec4<f32>(0.0, 0.0, 0.0, 0.0);
        })");
    desc.cFragment.module = utils::CreateShaderModule(device, R"(
        [[stage(fragment)]] fn main() -> void {
        })");

    desc.layout = pl;
    wgpu::RenderPipeline pipeline = device.CreateRenderPipeline2(&desc);

    desc.layout = samePl;
    wgpu::RenderPipeline samePipeline = device.CreateRenderPipeline2(&desc);

    desc.layout = otherPl;
    wgpu::RenderPipeline otherPipeline = device.CreateRenderPipeline2(&desc);

    EXPECT_NE(pipeline.Get(), otherPipeline.Get());
    EXPECT_EQ(pipeline.Get() == samePipeline.Get(), !UsesWire());
}

// Test that RenderPipelines are correctly deduplicated wrt. their vertex module
TEST_P(ObjectCachingTest, RenderPipelineDeduplicationOnVertexModule) {
    wgpu::ShaderModule module = utils::CreateShaderModule(device, R"(
        [[builtin(position)]] var<out> Position : vec4<f32>;
        [[stage(vertex)]] fn main() -> void {
            Position = vec4<f32>(0.0, 0.0, 0.0, 0.0);
        })");
    wgpu::ShaderModule sameModule = utils::CreateShaderModule(device, R"(
        [[builtin(position)]] var<out> Position : vec4<f32>;
        [[stage(vertex)]] fn main() -> void {
            Position = vec4<f32>(0.0, 0.0, 0.0, 0.0);
        })");
    wgpu::ShaderModule otherModule = utils::CreateShaderModule(device, R"(
        [[builtin(position)]] var<out> Position : vec4<f32>;
        [[stage(vertex)]] fn main() -> void {
            Position = vec4<f32>(1.0, 1.0, 1.0, 1.0);
        })");

    EXPECT_NE(module.Get(), otherModule.Get());
    EXPECT_EQ(module.Get() == sameModule.Get(), !UsesWire());

    utils::ComboRenderPipelineDescriptor2 desc;
    desc.cFragment.module = utils::CreateShaderModule(device, R"(
            [[stage(fragment)]] fn main() -> void {
            })");

    desc.vertex.module = module;
    wgpu::RenderPipeline pipeline = device.CreateRenderPipeline2(&desc);

    desc.vertex.module = sameModule;
    wgpu::RenderPipeline samePipeline = device.CreateRenderPipeline2(&desc);

    desc.vertex.module = otherModule;
    wgpu::RenderPipeline otherPipeline = device.CreateRenderPipeline2(&desc);

    EXPECT_NE(pipeline.Get(), otherPipeline.Get());
    EXPECT_EQ(pipeline.Get() == samePipeline.Get(), !UsesWire());
}

// Test that RenderPipelines are correctly deduplicated wrt. their fragment module
TEST_P(ObjectCachingTest, RenderPipelineDeduplicationOnFragmentModule) {
    wgpu::ShaderModule module = utils::CreateShaderModule(device, R"(
        [[stage(fragment)]] fn main() -> void {
        })");
    wgpu::ShaderModule sameModule = utils::CreateShaderModule(device, R"(
        [[stage(fragment)]] fn main() -> void {
        })");
    wgpu::ShaderModule otherModule = utils::CreateShaderModule(device, R"(
        [[location(0)]] var<out> fragColor : vec4<f32>;
        [[stage(fragment)]] fn main() -> void {
            fragColor = vec4<f32>(0.0, 0.0, 0.0, 0.0);
        })");

    EXPECT_NE(module.Get(), otherModule.Get());
    EXPECT_EQ(module.Get() == sameModule.Get(), !UsesWire());

    utils::ComboRenderPipelineDescriptor2 desc;
    desc.vertex.module = utils::CreateShaderModule(device, R"(
        [[builtin(position)]] var<out> Position : vec4<f32>;
        [[stage(vertex)]] fn main() -> void {
            Position = vec4<f32>(0.0, 0.0, 0.0, 0.0);
        })");

    desc.cFragment.module = module;
    wgpu::RenderPipeline pipeline = device.CreateRenderPipeline2(&desc);

    desc.cFragment.module = sameModule;
    wgpu::RenderPipeline samePipeline = device.CreateRenderPipeline2(&desc);

    desc.cFragment.module = otherModule;
    wgpu::RenderPipeline otherPipeline = device.CreateRenderPipeline2(&desc);

    EXPECT_NE(pipeline.Get(), otherPipeline.Get());
    EXPECT_EQ(pipeline.Get() == samePipeline.Get(), !UsesWire());
}

// Test that Samplers are correctly deduplicated.
TEST_P(ObjectCachingTest, SamplerDeduplication) {
    wgpu::SamplerDescriptor samplerDesc;
    wgpu::Sampler sampler = device.CreateSampler(&samplerDesc);

    wgpu::SamplerDescriptor sameSamplerDesc;
    wgpu::Sampler sameSampler = device.CreateSampler(&sameSamplerDesc);

    wgpu::SamplerDescriptor otherSamplerDescAddressModeU;
    otherSamplerDescAddressModeU.addressModeU = wgpu::AddressMode::Repeat;
    wgpu::Sampler otherSamplerAddressModeU = device.CreateSampler(&otherSamplerDescAddressModeU);

    wgpu::SamplerDescriptor otherSamplerDescAddressModeV;
    otherSamplerDescAddressModeV.addressModeV = wgpu::AddressMode::Repeat;
    wgpu::Sampler otherSamplerAddressModeV = device.CreateSampler(&otherSamplerDescAddressModeV);

    wgpu::SamplerDescriptor otherSamplerDescAddressModeW;
    otherSamplerDescAddressModeW.addressModeW = wgpu::AddressMode::Repeat;
    wgpu::Sampler otherSamplerAddressModeW = device.CreateSampler(&otherSamplerDescAddressModeW);

    wgpu::SamplerDescriptor otherSamplerDescMagFilter;
    otherSamplerDescMagFilter.magFilter = wgpu::FilterMode::Linear;
    wgpu::Sampler otherSamplerMagFilter = device.CreateSampler(&otherSamplerDescMagFilter);

    wgpu::SamplerDescriptor otherSamplerDescMinFilter;
    otherSamplerDescMinFilter.minFilter = wgpu::FilterMode::Linear;
    wgpu::Sampler otherSamplerMinFilter = device.CreateSampler(&otherSamplerDescMinFilter);

    wgpu::SamplerDescriptor otherSamplerDescMipmapFilter;
    otherSamplerDescMipmapFilter.mipmapFilter = wgpu::FilterMode::Linear;
    wgpu::Sampler otherSamplerMipmapFilter = device.CreateSampler(&otherSamplerDescMipmapFilter);

    wgpu::SamplerDescriptor otherSamplerDescLodMinClamp;
    otherSamplerDescLodMinClamp.lodMinClamp += 1;
    wgpu::Sampler otherSamplerLodMinClamp = device.CreateSampler(&otherSamplerDescLodMinClamp);

    wgpu::SamplerDescriptor otherSamplerDescLodMaxClamp;
    otherSamplerDescLodMaxClamp.lodMaxClamp += 1;
    wgpu::Sampler otherSamplerLodMaxClamp = device.CreateSampler(&otherSamplerDescLodMaxClamp);

    wgpu::SamplerDescriptor otherSamplerDescCompareFunction;
    otherSamplerDescCompareFunction.compare = wgpu::CompareFunction::Always;
    wgpu::Sampler otherSamplerCompareFunction =
        device.CreateSampler(&otherSamplerDescCompareFunction);

    EXPECT_NE(sampler.Get(), otherSamplerAddressModeU.Get());
    EXPECT_NE(sampler.Get(), otherSamplerAddressModeV.Get());
    EXPECT_NE(sampler.Get(), otherSamplerAddressModeW.Get());
    EXPECT_NE(sampler.Get(), otherSamplerMagFilter.Get());
    EXPECT_NE(sampler.Get(), otherSamplerMinFilter.Get());
    EXPECT_NE(sampler.Get(), otherSamplerMipmapFilter.Get());
    EXPECT_NE(sampler.Get(), otherSamplerLodMinClamp.Get());
    EXPECT_NE(sampler.Get(), otherSamplerLodMaxClamp.Get());
    EXPECT_NE(sampler.Get(), otherSamplerCompareFunction.Get());
    EXPECT_EQ(sampler.Get() == sameSampler.Get(), !UsesWire());
}

DAWN_INSTANTIATE_TEST(ObjectCachingTest,
                      D3D12Backend(),
                      MetalBackend(),
                      OpenGLBackend(),
                      OpenGLESBackend(),
                      VulkanBackend());
