// Copyright 2021 The Dawn Authors
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

#include "dawn/tests/DawnTest.h"

#include "dawn/utils/ComboRenderPipelineDescriptor.h"
#include "dawn/utils/TextureUtils.h"
#include "dawn/utils/WGPUHelpers.h"

constexpr static uint32_t kSize = 4;

namespace {
    using TextureFormat = wgpu::TextureFormat;
    DAWN_TEST_PARAM_STRUCT(ReadOnlyDepthStencilAttachmentTestsParams, TextureFormat);
}  // namespace

class ReadOnlyDepthStencilAttachmentTests
    : public DawnTestWithParams<ReadOnlyDepthStencilAttachmentTestsParams> {
  protected:
    struct DepthStencilValues {
        float depthInitValue;
        uint32_t stencilInitValue;
        uint32_t stencilRefValue;
    };

    std::vector<wgpu::FeatureName> GetRequiredFeatures() override {
        switch (GetParam().mTextureFormat) {
            case wgpu::TextureFormat::Depth24UnormStencil8:
                if (SupportsFeatures({wgpu::FeatureName::Depth24UnormStencil8})) {
                    mIsFormatSupported = true;
                    return {wgpu::FeatureName::Depth24UnormStencil8};
                }

                return {};
            case wgpu::TextureFormat::Depth32FloatStencil8:
                if (SupportsFeatures({wgpu::FeatureName::Depth32FloatStencil8})) {
                    mIsFormatSupported = true;
                    return {wgpu::FeatureName::Depth32FloatStencil8};
                }

                return {};
            default:
                mIsFormatSupported = true;
                return {};
        }
    }

    bool IsFormatSupported() const {
        return mIsFormatSupported;
    }

    wgpu::RenderPipeline CreateRenderPipeline(wgpu::TextureAspect aspect,
                                              wgpu::TextureFormat format,
                                              bool sampleFromAttachment) {
        utils::ComboRenderPipelineDescriptor pipelineDescriptor;

        // Draw a rectangle via two triangles. The depth value of the top of the rectangle is 0.4.
        // The depth value of the bottom is 0.0. The depth value gradually change from 0.4 to 0.0
        // from the top to the bottom. The top part will compare with the depth values and fail to
        // pass the depth test. The bottom part will compare with the depth values in depth buffer
        // and pass the depth test, and sample from the depth buffer in fragment shader in the same
        // pipeline.
        pipelineDescriptor.vertex.module = utils::CreateShaderModule(device, R"(
            @stage(vertex)
            fn main(@builtin(vertex_index) VertexIndex : u32) -> @builtin(position) vec4<f32> {
                var pos = array<vec3<f32>, 6>(
                    vec3<f32>(-1.0,  1.0, 0.4),
                    vec3<f32>(-1.0, -1.0, 0.0),
                    vec3<f32>( 1.0,  1.0, 0.4),
                    vec3<f32>( 1.0,  1.0, 0.4),
                    vec3<f32>(-1.0, -1.0, 0.0),
                    vec3<f32>( 1.0, -1.0, 0.0));
                return vec4<f32>(pos[VertexIndex], 1.0);
            })");

        if (!sampleFromAttachment) {
            // Draw a solid blue into color buffer if not sample from depth/stencil attachment.
            pipelineDescriptor.cFragment.module = utils::CreateShaderModule(device, R"(
            @stage(fragment) fn main() -> @location(0) vec4<f32> {
                return vec4<f32>(0.0, 0.0, 1.0, 0.0);
            })");
        } else {
            // Sample from depth/stencil attachment and draw that sampled texel into color buffer.
            if (aspect == wgpu::TextureAspect::DepthOnly) {
                pipelineDescriptor.cFragment.module = utils::CreateShaderModule(device, R"(
                @group(0) @binding(0) var samp : sampler;
                @group(0) @binding(1) var tex : texture_depth_2d;

                @stage(fragment)
                fn main(@builtin(position) FragCoord : vec4<f32>) -> @location(0) vec4<f32> {
                    return vec4<f32>(textureSample(tex, samp, FragCoord.xy), 0.0, 0.0, 0.0);
                })");
            } else {
                ASSERT(aspect == wgpu::TextureAspect::StencilOnly);
                pipelineDescriptor.cFragment.module = utils::CreateShaderModule(device, R"(
                @group(0) @binding(0) var tex : texture_2d<u32>;

                @stage(fragment)
                fn main(@builtin(position) FragCoord : vec4<f32>) -> @location(0) vec4<f32> {
                    var texel = textureLoad(tex, vec2<i32>(FragCoord.xy), 0);
                    return vec4<f32>(f32(texel[0]) / 255.0, 0.0, 0.0, 0.0);
                })");
            }
        }

        // Enable depth or stencil test. But depth/stencil write is not enabled.
        wgpu::DepthStencilState* depthStencil = pipelineDescriptor.EnableDepthStencil(format);
        if (aspect == wgpu::TextureAspect::DepthOnly) {
            depthStencil->depthCompare = wgpu::CompareFunction::LessEqual;
        } else {
            depthStencil->stencilFront.compare = wgpu::CompareFunction::LessEqual;
        }

        return device.CreateRenderPipeline(&pipelineDescriptor);
    }

    wgpu::Texture CreateTexture(wgpu::TextureFormat format, wgpu::TextureUsage usage) {
        wgpu::TextureDescriptor descriptor = {};
        descriptor.size = {kSize, kSize, 1};
        descriptor.format = format;
        descriptor.usage = usage;
        return device.CreateTexture(&descriptor);
    }

    void DoTest(wgpu::TextureAspect aspect,
                wgpu::TextureFormat format,
                wgpu::Texture colorTexture,
                DepthStencilValues* values,
                bool sampleFromAttachment) {
        wgpu::TextureUsage dsTextureUsage = wgpu::TextureUsage::RenderAttachment;
        if (sampleFromAttachment) {
            dsTextureUsage |= wgpu::TextureUsage::TextureBinding;
        }
        wgpu::Texture depthStencilTexture = CreateTexture(format, dsTextureUsage);

        wgpu::CommandEncoder commandEncoder = device.CreateCommandEncoder();

        // Note that we must encompass all aspects for texture view used in attachment.
        wgpu::TextureView depthStencilViewInAttachment = depthStencilTexture.CreateView();
        utils::ComboRenderPassDescriptor passDescriptorInit({}, depthStencilViewInAttachment);
        passDescriptorInit.UnsetDepthStencilLoadStoreOpsForFormat(format);
        if (aspect == wgpu::TextureAspect::DepthOnly) {
            passDescriptorInit.cDepthStencilAttachmentInfo.depthClearValue = values->depthInitValue;
        } else {
            ASSERT(aspect == wgpu::TextureAspect::StencilOnly);
            passDescriptorInit.cDepthStencilAttachmentInfo.stencilClearValue =
                values->stencilInitValue;
        }
        wgpu::RenderPassEncoder passInit = commandEncoder.BeginRenderPass(&passDescriptorInit);
        passInit.End();

        // Note that we can only select one single aspect for texture view used in bind group.
        wgpu::TextureViewDescriptor viewDesc = {};
        viewDesc.aspect = aspect;
        wgpu::TextureView depthStencilViewInBindGroup = depthStencilTexture.CreateView(&viewDesc);

        // Create a render pass to initialize the depth/stencil attachment.
        utils::ComboRenderPassDescriptor passDescriptor({colorTexture.CreateView()},
                                                        depthStencilViewInAttachment);
        // Set both aspects to readonly. We have to do this if the format has both aspects, or
        // it doesn't impact anything if the format has only one aspect.
        passDescriptor.cDepthStencilAttachmentInfo.depthReadOnly = true;
        passDescriptor.cDepthStencilAttachmentInfo.depthLoadOp = wgpu::LoadOp::Undefined;
        passDescriptor.cDepthStencilAttachmentInfo.depthStoreOp = wgpu::StoreOp::Undefined;
        passDescriptor.cDepthStencilAttachmentInfo.stencilReadOnly = true;
        passDescriptor.cDepthStencilAttachmentInfo.stencilLoadOp = wgpu::LoadOp::Undefined;
        passDescriptor.cDepthStencilAttachmentInfo.stencilStoreOp = wgpu::StoreOp::Undefined;

        // Create a render pass with readonly depth/stencil attachment. The attachment has already
        // been initialized. The pipeline in this render pass will sample from the attachment.
        // The pipeline will read from the attachment to do depth/stencil test too.
        wgpu::RenderPassEncoder pass = commandEncoder.BeginRenderPass(&passDescriptor);
        wgpu::RenderPipeline pipeline = CreateRenderPipeline(aspect, format, sampleFromAttachment);
        pass.SetPipeline(pipeline);
        if (aspect == wgpu::TextureAspect::DepthOnly) {
            if (sampleFromAttachment) {
                wgpu::BindGroup bindGroup = utils::MakeBindGroup(
                    device, pipeline.GetBindGroupLayout(0),
                    {{0, device.CreateSampler()}, {1, depthStencilViewInBindGroup}});
                pass.SetBindGroup(0, bindGroup);
            }
        } else {
            ASSERT(aspect == wgpu::TextureAspect::StencilOnly);
            if (sampleFromAttachment) {
                wgpu::BindGroup bindGroup = utils::MakeBindGroup(
                    device, pipeline.GetBindGroupLayout(0), {{0, depthStencilViewInBindGroup}});
                pass.SetBindGroup(0, bindGroup);
            }
            pass.SetStencilReference(values->stencilRefValue);
        }
        pass.Draw(6);
        pass.End();

        wgpu::CommandBuffer commands = commandEncoder.Finish();
        queue.Submit(1, &commands);
    }

  private:
    bool mIsFormatSupported = false;
};

class ReadOnlyDepthAttachmentTests : public ReadOnlyDepthStencilAttachmentTests {
  protected:
    void SetUp() override {
        ReadOnlyDepthStencilAttachmentTests::SetUp();
        DAWN_TEST_UNSUPPORTED_IF(!IsFormatSupported());
    }
};

TEST_P(ReadOnlyDepthAttachmentTests, SampleFromAttachment) {
    wgpu::Texture colorTexture =
        CreateTexture(wgpu::TextureFormat::RGBA8Unorm,
                      wgpu::TextureUsage::RenderAttachment | wgpu::TextureUsage::CopySrc);

    wgpu::TextureFormat depthFormat = GetParam().mTextureFormat;

    DepthStencilValues values;
    values.depthInitValue = 0.2;

    DoTest(wgpu::TextureAspect::DepthOnly, depthFormat, colorTexture, &values, true);

    // The top part is not rendered by the pipeline. Its color is the default clear color for
    // color attachment.
    const std::vector<RGBA8> kExpectedTopColors(kSize * kSize / 2, {0, 0, 0, 0});
    // The bottom part is rendered, whose red channel is sampled from depth attachment, which
    // is initialized into 0.2.
    const std::vector<RGBA8> kExpectedBottomColors(kSize * kSize / 2,
                                                   {static_cast<uint8_t>(0.2 * 255), 0, 0, 0});
    EXPECT_TEXTURE_EQ(kExpectedTopColors.data(), colorTexture, {0, 0}, {kSize, kSize / 2});
    EXPECT_TEXTURE_EQ(kExpectedBottomColors.data(), colorTexture, {0, kSize / 2},
                      {kSize, kSize / 2});
}

TEST_P(ReadOnlyDepthAttachmentTests, NotSampleFromAttachment) {
    wgpu::Texture colorTexture =
        CreateTexture(wgpu::TextureFormat::RGBA8Unorm,
                      wgpu::TextureUsage::RenderAttachment | wgpu::TextureUsage::CopySrc);

    wgpu::TextureFormat depthFormat = GetParam().mTextureFormat;

    DepthStencilValues values;
    values.depthInitValue = 0.2;

    DoTest(wgpu::TextureAspect::DepthOnly, depthFormat, colorTexture, &values, false);

    // The top part is not rendered by the pipeline. Its color is the default clear color for
    // color attachment.
    const std::vector<RGBA8> kExpectedTopColors(kSize * kSize / 2, {0, 0, 0, 0});
    // The bottom part is rendered. Its color is set to blue.
    const std::vector<RGBA8> kExpectedBottomColors(kSize * kSize / 2, {0, 0, 255, 0});
    EXPECT_TEXTURE_EQ(kExpectedTopColors.data(), colorTexture, {0, 0}, {kSize, kSize / 2});
    EXPECT_TEXTURE_EQ(kExpectedBottomColors.data(), colorTexture, {0, kSize / 2},
                      {kSize, kSize / 2});
}

class ReadOnlyStencilAttachmentTests : public ReadOnlyDepthStencilAttachmentTests {
  protected:
    void SetUp() override {
        ReadOnlyDepthStencilAttachmentTests::SetUp();
        DAWN_TEST_UNSUPPORTED_IF(!IsFormatSupported());
    }
};

TEST_P(ReadOnlyStencilAttachmentTests, SampleFromAttachment) {
    wgpu::Texture colorTexture =
        CreateTexture(wgpu::TextureFormat::RGBA8Unorm,
                      wgpu::TextureUsage::RenderAttachment | wgpu::TextureUsage::CopySrc);

    wgpu::TextureFormat stencilFormat = GetParam().mTextureFormat;

    DepthStencilValues values;
    values.stencilInitValue = 3;
    values.stencilRefValue = 2;
    // stencilRefValue < stencilValue (stencilInitValue), so stencil test passes. The pipeline
    // samples from stencil buffer and writes into color buffer.
    DoTest(wgpu::TextureAspect::StencilOnly, stencilFormat, colorTexture, &values, true);
    const std::vector<RGBA8> kSampledColors(kSize * kSize, {3, 0, 0, 0});
    EXPECT_TEXTURE_EQ(kSampledColors.data(), colorTexture, {0, 0}, {kSize, kSize});

    values.stencilInitValue = 1;
    // stencilRefValue > stencilValue (stencilInitValue), so stencil test fails. The pipeline
    // doesn't change color buffer. Sampled data from stencil buffer is discarded.
    DoTest(wgpu::TextureAspect::StencilOnly, stencilFormat, colorTexture, &values, true);
    const std::vector<RGBA8> kInitColors(kSize * kSize, {0, 0, 0, 0});
    EXPECT_TEXTURE_EQ(kInitColors.data(), colorTexture, {0, 0}, {kSize, kSize});
}

TEST_P(ReadOnlyStencilAttachmentTests, NotSampleFromAttachment) {
    wgpu::Texture colorTexture =
        CreateTexture(wgpu::TextureFormat::RGBA8Unorm,
                      wgpu::TextureUsage::RenderAttachment | wgpu::TextureUsage::CopySrc);

    wgpu::TextureFormat stencilFormat = GetParam().mTextureFormat;

    DepthStencilValues values;
    values.stencilInitValue = 3;
    values.stencilRefValue = 2;
    // stencilRefValue < stencilValue (stencilInitValue), so stencil test passes. The pipeline
    // draw solid blue into color buffer.
    DoTest(wgpu::TextureAspect::StencilOnly, stencilFormat, colorTexture, &values, false);
    const std::vector<RGBA8> kSampledColors(kSize * kSize, {0, 0, 255, 0});
    EXPECT_TEXTURE_EQ(kSampledColors.data(), colorTexture, {0, 0}, {kSize, kSize});

    values.stencilInitValue = 1;
    // stencilRefValue > stencilValue (stencilInitValue), so stencil test fails. The pipeline
    // doesn't change color buffer. drawing data is discarded.
    DoTest(wgpu::TextureAspect::StencilOnly, stencilFormat, colorTexture, &values, false);
    const std::vector<RGBA8> kInitColors(kSize * kSize, {0, 0, 0, 0});
    EXPECT_TEXTURE_EQ(kInitColors.data(), colorTexture, {0, 0}, {kSize, kSize});
}

DAWN_INSTANTIATE_TEST_P(ReadOnlyDepthAttachmentTests,
                        {D3D12Backend(), D3D12Backend({}, {"use_d3d12_render_pass"}),
                         MetalBackend(), VulkanBackend()},
                        std::vector<wgpu::TextureFormat>(utils::kDepthFormats.begin(),
                                                         utils::kDepthFormats.end()));
DAWN_INSTANTIATE_TEST_P(ReadOnlyStencilAttachmentTests,
                        {D3D12Backend(), D3D12Backend({}, {"use_d3d12_render_pass"}),
                         MetalBackend(), VulkanBackend()},
                        std::vector<wgpu::TextureFormat>(utils::kStencilFormats.begin(),
                                                         utils::kStencilFormats.end()));
