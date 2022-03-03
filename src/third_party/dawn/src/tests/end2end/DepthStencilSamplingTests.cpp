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

#include "common/Assert.h"
#include "tests/DawnTest.h"
#include "utils/ComboRenderPipelineDescriptor.h"
#include "utils/WGPUHelpers.h"

namespace {

    constexpr wgpu::TextureFormat kDepthFormats[] = {
        wgpu::TextureFormat::Depth32Float,
        wgpu::TextureFormat::Depth24Plus,
        wgpu::TextureFormat::Depth24PlusStencil8,
        wgpu::TextureFormat::Depth16Unorm,
    };

    constexpr wgpu::TextureFormat kStencilFormats[] = {
        wgpu::TextureFormat::Depth24PlusStencil8,
    };

    constexpr wgpu::CompareFunction kCompareFunctions[] = {
        wgpu::CompareFunction::Never,        wgpu::CompareFunction::Less,
        wgpu::CompareFunction::LessEqual,    wgpu::CompareFunction::Greater,
        wgpu::CompareFunction::GreaterEqual, wgpu::CompareFunction::Equal,
        wgpu::CompareFunction::NotEqual,     wgpu::CompareFunction::Always,
    };

    // Test a "normal" ref value between 0 and 1; as well as negative and > 1 refs.
    constexpr float kCompareRefs[] = {-0.1, 0.4, 1.2};

    // Test 0, below the ref, equal to, above the ref, and 1.
    const std::vector<float> kNormalizedTextureValues = {0.0, 0.3, 0.4, 0.5, 1.0};

    // Test the limits, and some values in between.
    const std::vector<uint32_t> kStencilValues = {0, 1, 38, 255};

}  // anonymous namespace

class DepthStencilSamplingTest : public DawnTest {
  protected:
    enum class TestAspect {
        Depth,
        Stencil,
    };

    void SetUp() override {
        DawnTest::SetUp();

        wgpu::BufferDescriptor uniformBufferDesc;
        uniformBufferDesc.usage = wgpu::BufferUsage::Uniform | wgpu::BufferUsage::CopyDst;
        uniformBufferDesc.size = sizeof(float);
        mUniformBuffer = device.CreateBuffer(&uniformBufferDesc);
    }

    void GenerateSamplingShader(const std::vector<TestAspect>& aspects,
                                const std::vector<uint32_t> components,
                                std::ostringstream& shaderSource,
                                std::ostringstream& shaderBody) {
        shaderSource << "type StencilValues = array<u32, " << components.size() << ">;\n";
        shaderSource << R"(
            [[block]] struct DepthResult {
                value : f32;
            };
            [[block]] struct StencilResult {
                values : StencilValues;
            };)";
        shaderSource << "\n";

        uint32_t index = 0;
        for (TestAspect aspect : aspects) {
            switch (aspect) {
                case TestAspect::Depth:
                    shaderSource << "[[group(0), binding(" << 2 * index << ")]] var tex" << index
                                 << " : texture_depth_2d;\n";

                    shaderSource << "[[group(0), binding(" << 2 * index + 1
                                 << ")]] var<storage, read_write> result" << index
                                 << " : DepthResult;\n";

                    ASSERT(components.size() == 1 && components[0] == 0);
                    shaderBody << "\nresult" << index << ".value = textureLoad(tex" << index
                               << ", vec2<i32>(0, 0), 0);";
                    break;
                case TestAspect::Stencil:
                    shaderSource << "[[group(0), binding(" << 2 * index << ")]] var tex" << index
                                 << " : texture_2d<u32>;\n";

                    shaderSource << "[[group(0), binding(" << 2 * index + 1
                                 << ")]] var<storage, read_write> result" << index
                                 << " : StencilResult;\n";

                    shaderBody << "var texel = textureLoad(tex" << index
                               << ", vec2<i32>(0, 0), 0);";

                    for (uint32_t i = 0; i < components.size(); ++i) {
                        shaderBody << "\nresult" << index << ".values[" << i << "] = texel["
                                   << components[i] << "];";
                    }
                    break;
            }

            index++;
        }
    }

    wgpu::RenderPipeline CreateSamplingRenderPipeline(std::vector<TestAspect> aspects,
                                                      std::vector<uint32_t> components) {
        wgpu::ShaderModule vsModule = utils::CreateShaderModule(device, R"(
            [[stage(vertex)]] fn main() -> [[builtin(position)]] vec4<f32> {
                return vec4<f32>(0.0, 0.0, 0.0, 1.0);
            })");

        utils::ComboRenderPipelineDescriptor pipelineDescriptor;

        std::ostringstream shaderSource;
        std::ostringstream shaderOutputStruct;
        std::ostringstream shaderBody;

        GenerateSamplingShader(aspects, components, shaderSource, shaderBody);

        shaderSource << "[[stage(fragment)]] fn main() -> [[location(0)]] vec4<f32> {\n";
        shaderSource << shaderBody.str() << "return vec4<f32>();\n }";

        wgpu::ShaderModule fsModule = utils::CreateShaderModule(device, shaderSource.str().c_str());
        pipelineDescriptor.vertex.module = vsModule;
        pipelineDescriptor.cFragment.module = fsModule;
        pipelineDescriptor.primitive.topology = wgpu::PrimitiveTopology::PointList;
        pipelineDescriptor.cTargets[0].writeMask = wgpu::ColorWriteMask::None;

        return device.CreateRenderPipeline(&pipelineDescriptor);
    }

    wgpu::ComputePipeline CreateSamplingComputePipeline(std::vector<TestAspect> aspects,
                                                        std::vector<uint32_t> components) {
        std::ostringstream shaderSource;
        std::ostringstream shaderBody;
        GenerateSamplingShader(aspects, components, shaderSource, shaderBody);

        shaderSource << "[[stage(compute), workgroup_size(1)]] fn main() { " << shaderBody.str()
                     << "\n}";

        wgpu::ShaderModule csModule = utils::CreateShaderModule(device, shaderSource.str().c_str());

        wgpu::ComputePipelineDescriptor pipelineDescriptor;
        pipelineDescriptor.compute.module = csModule;
        pipelineDescriptor.compute.entryPoint = "main";

        return device.CreateComputePipeline(&pipelineDescriptor);
    }

    wgpu::RenderPipeline CreateSamplingRenderPipeline(std::vector<TestAspect> aspects,
                                                      uint32_t componentIndex) {
        return CreateSamplingRenderPipeline(std::move(aspects),
                                            std::vector<uint32_t>{componentIndex});
    }

    wgpu::ComputePipeline CreateSamplingComputePipeline(std::vector<TestAspect> aspects,
                                                        uint32_t componentIndex) {
        return CreateSamplingComputePipeline(std::move(aspects),
                                             std::vector<uint32_t>{componentIndex});
    }

    wgpu::RenderPipeline CreateComparisonRenderPipeline() {
        wgpu::ShaderModule vsModule = utils::CreateShaderModule(device, R"(
            [[stage(vertex)]] fn main() -> [[builtin(position)]] vec4<f32> {
                return vec4<f32>(0.0, 0.0, 0.0, 1.0);
            })");

        wgpu::ShaderModule fsModule = utils::CreateShaderModule(device, R"(
            [[group(0), binding(0)]] var samp : sampler_comparison;
            [[group(0), binding(1)]] var tex : texture_depth_2d;
            [[block]] struct Uniforms {
                compareRef : f32;
            };
            [[group(0), binding(2)]] var<uniform> uniforms : Uniforms;

            [[stage(fragment)]] fn main() -> [[location(0)]] f32 {
                return textureSampleCompare(tex, samp, vec2<f32>(0.5, 0.5), uniforms.compareRef);
            })");

        utils::ComboRenderPipelineDescriptor pipelineDescriptor;
        pipelineDescriptor.vertex.module = vsModule;
        pipelineDescriptor.cFragment.module = fsModule;
        pipelineDescriptor.primitive.topology = wgpu::PrimitiveTopology::PointList;
        pipelineDescriptor.cTargets[0].format = wgpu::TextureFormat::R32Float;

        return device.CreateRenderPipeline(&pipelineDescriptor);
    }

    wgpu::ComputePipeline CreateComparisonComputePipeline() {
        wgpu::ShaderModule csModule = utils::CreateShaderModule(device, R"(
            [[group(0), binding(0)]] var samp : sampler_comparison;
            [[group(0), binding(1)]] var tex : texture_depth_2d;
            [[block]] struct Uniforms {
                compareRef : f32;
            };
            [[group(0), binding(2)]] var<uniform> uniforms : Uniforms;

            [[block]] struct SamplerResult {
                value : f32;
            };
            [[group(0), binding(3)]] var<storage, read_write> samplerResult : SamplerResult;

            [[stage(compute), workgroup_size(1)]] fn main() {
                samplerResult.value = textureSampleCompare(tex, samp, vec2<f32>(0.5, 0.5), uniforms.compareRef);
            })");

        wgpu::ComputePipelineDescriptor pipelineDescriptor;
        pipelineDescriptor.compute.module = csModule;
        pipelineDescriptor.compute.entryPoint = "main";

        return device.CreateComputePipeline(&pipelineDescriptor);
    }

    wgpu::Texture CreateInputTexture(wgpu::TextureFormat format) {
        wgpu::TextureDescriptor inputTextureDesc;
        inputTextureDesc.usage =
            wgpu::TextureUsage::TextureBinding | wgpu::TextureUsage::RenderAttachment;
        inputTextureDesc.size = {1, 1, 1};
        inputTextureDesc.format = format;
        return device.CreateTexture(&inputTextureDesc);
    }

    wgpu::Texture CreateOutputTexture(wgpu::TextureFormat format) {
        wgpu::TextureDescriptor outputTextureDesc;
        outputTextureDesc.usage =
            wgpu::TextureUsage::RenderAttachment | wgpu::TextureUsage::CopySrc;
        outputTextureDesc.size = {1, 1, 1};
        outputTextureDesc.format = format;
        return device.CreateTexture(&outputTextureDesc);
    }

    wgpu::Buffer CreateOutputBuffer(uint32_t componentCount = 1) {
        wgpu::BufferDescriptor outputBufferDesc;
        outputBufferDesc.usage = wgpu::BufferUsage::Storage | wgpu::BufferUsage::CopySrc;
        outputBufferDesc.size = sizeof(uint32_t) * componentCount;
        return device.CreateBuffer(&outputBufferDesc);
    }

    void UpdateInputDepth(wgpu::CommandEncoder commandEncoder,
                          wgpu::Texture texture,
                          float depthValue) {
        utils::ComboRenderPassDescriptor passDescriptor({}, texture.CreateView());
        passDescriptor.cDepthStencilAttachmentInfo.clearDepth = depthValue;

        wgpu::RenderPassEncoder pass = commandEncoder.BeginRenderPass(&passDescriptor);
        pass.EndPass();
    }

    void UpdateInputStencil(wgpu::CommandEncoder commandEncoder,
                            wgpu::Texture texture,
                            uint8_t stencilValue) {
        utils::ComboRenderPassDescriptor passDescriptor({}, texture.CreateView());
        passDescriptor.cDepthStencilAttachmentInfo.clearStencil = stencilValue;

        wgpu::RenderPassEncoder pass = commandEncoder.BeginRenderPass(&passDescriptor);
        pass.EndPass();
    }

    template <typename T, typename CheckBufferFn>
    void DoSamplingTestImpl(TestAspect aspect,
                            wgpu::RenderPipeline pipeline,
                            wgpu::TextureFormat format,
                            std::vector<T> textureValues,
                            uint32_t componentCount,
                            CheckBufferFn CheckBuffer) {
        wgpu::Texture inputTexture = CreateInputTexture(format);
        wgpu::TextureViewDescriptor inputViewDesc = {};
        switch (aspect) {
            case TestAspect::Depth:
                inputViewDesc.aspect = wgpu::TextureAspect::DepthOnly;
                break;
            case TestAspect::Stencil:
                inputViewDesc.aspect = wgpu::TextureAspect::StencilOnly;
                break;
        }

        wgpu::Buffer outputBuffer = CreateOutputBuffer(componentCount);

        wgpu::BindGroup bindGroup =
            utils::MakeBindGroup(device, pipeline.GetBindGroupLayout(0),
                                 {{0, inputTexture.CreateView(&inputViewDesc)}, {1, outputBuffer}});

        for (size_t i = 0; i < textureValues.size(); ++i) {
            // Set the input depth texture to the provided texture value
            wgpu::CommandEncoder commandEncoder = device.CreateCommandEncoder();
            switch (aspect) {
                case TestAspect::Depth:
                    UpdateInputDepth(commandEncoder, inputTexture, textureValues[i]);
                    break;
                case TestAspect::Stencil:
                    UpdateInputStencil(commandEncoder, inputTexture, textureValues[i]);
                    break;
            }

            // Render into the output texture
            {
                utils::BasicRenderPass renderPass =
                    utils::CreateBasicRenderPass(device, 1, 1, wgpu::TextureFormat::RGBA8Unorm);
                wgpu::RenderPassEncoder pass =
                    commandEncoder.BeginRenderPass(&renderPass.renderPassInfo);
                pass.SetPipeline(pipeline);
                pass.SetBindGroup(0, bindGroup);
                pass.Draw(1);
                pass.EndPass();
            }

            wgpu::CommandBuffer commands = commandEncoder.Finish();
            queue.Submit(1, &commands);

            CheckBuffer(textureValues[i], outputBuffer);
        }
    }

    template <typename T, typename CheckBufferFn>
    void DoSamplingTestImpl(TestAspect aspect,
                            wgpu::ComputePipeline pipeline,
                            wgpu::TextureFormat format,
                            std::vector<T> textureValues,
                            uint32_t componentCount,
                            CheckBufferFn CheckBuffer) {
        wgpu::Texture inputTexture = CreateInputTexture(format);
        wgpu::TextureViewDescriptor inputViewDesc = {};
        switch (aspect) {
            case TestAspect::Depth:
                inputViewDesc.aspect = wgpu::TextureAspect::DepthOnly;
                break;
            case TestAspect::Stencil:
                inputViewDesc.aspect = wgpu::TextureAspect::StencilOnly;
                break;
        }

        wgpu::Buffer outputBuffer = CreateOutputBuffer(componentCount);

        wgpu::BindGroup bindGroup =
            utils::MakeBindGroup(device, pipeline.GetBindGroupLayout(0),
                                 {{0, inputTexture.CreateView(&inputViewDesc)}, {1, outputBuffer}});

        for (size_t i = 0; i < textureValues.size(); ++i) {
            // Set the input depth texture to the provided texture value
            wgpu::CommandEncoder commandEncoder = device.CreateCommandEncoder();
            switch (aspect) {
                case TestAspect::Depth:
                    UpdateInputDepth(commandEncoder, inputTexture, textureValues[i]);
                    break;
                case TestAspect::Stencil:
                    UpdateInputStencil(commandEncoder, inputTexture, textureValues[i]);
                    break;
            }

            // Sample into the output buffer
            {
                wgpu::ComputePassEncoder pass = commandEncoder.BeginComputePass();
                pass.SetPipeline(pipeline);
                pass.SetBindGroup(0, bindGroup);
                pass.Dispatch(1);
                pass.EndPass();
            }

            wgpu::CommandBuffer commands = commandEncoder.Finish();
            queue.Submit(1, &commands);

            CheckBuffer(textureValues[i], outputBuffer);
        }
    }

    template <typename T>
    void DoSamplingTest(TestAspect aspect,
                        wgpu::RenderPipeline pipeline,
                        wgpu::TextureFormat format,
                        std::vector<T> textureValues,
                        T tolerance = {}) {
        DoSamplingTestImpl(aspect, pipeline, format, textureValues, 1,
                           [this, tolerance](T expected, wgpu::Buffer buffer) {
                               EXPECT_BUFFER(buffer, 0, sizeof(T),
                                             new ::detail::ExpectEq<T>(expected, tolerance));
                           });
    }

    template <typename T>
    void DoSamplingTest(TestAspect aspect,
                        wgpu::ComputePipeline pipeline,
                        wgpu::TextureFormat format,
                        std::vector<T> textureValues,
                        T tolerance = {}) {
        DoSamplingTestImpl(aspect, pipeline, format, textureValues, 1,
                           [this, tolerance](T expected, wgpu::Buffer buffer) {
                               EXPECT_BUFFER(buffer, 0, sizeof(T),
                                             new ::detail::ExpectEq<T>(expected, tolerance));
                           });
    }

    class ExtraStencilComponentsExpectation : public detail::Expectation {
        using StencilData = std::array<uint32_t, 4>;

      public:
        ExtraStencilComponentsExpectation(uint32_t expected) : mExpected(expected) {
        }

        ~ExtraStencilComponentsExpectation() override = default;

        testing::AssertionResult Check(const void* rawData, size_t size) override {
            ASSERT(size == sizeof(StencilData));
            const uint32_t* data = static_cast<const uint32_t*>(rawData);

            StencilData ssss = {mExpected, mExpected, mExpected, mExpected};
            StencilData s001 = {mExpected, 0, 0, 1};

            if (memcmp(data, ssss.data(), size) == 0 || memcmp(data, s001.data(), size) == 0) {
                return testing::AssertionSuccess();
            }

            return testing::AssertionFailure() << "Expected stencil data to be "
                                               << "(" << ssss[0] << ", " << ssss[1] << ", "
                                               << ssss[2] << ", " << ssss[3] << ") or "
                                               << "(" << s001[0] << ", " << s001[1] << ", "
                                               << s001[2] << ", " << s001[3] << "). Got "
                                               << "(" << data[0] << ", " << data[1] << ", "
                                               << data[2] << ", " << data[3] << ").";
        }

      private:
        uint32_t mExpected;
    };

    void DoSamplingExtraStencilComponentsRenderTest(TestAspect aspect,
                                                    wgpu::TextureFormat format,
                                                    std::vector<uint8_t> textureValues) {
        DoSamplingTestImpl(aspect,
                           CreateSamplingRenderPipeline({TestAspect::Stencil}, {0, 1, 2, 3}),
                           format, textureValues, 4, [&](uint32_t expected, wgpu::Buffer buffer) {
                               EXPECT_BUFFER(buffer, 0, 4 * sizeof(uint32_t),
                                             new ExtraStencilComponentsExpectation(expected));
                           });
    }

    void DoSamplingExtraStencilComponentsComputeTest(TestAspect aspect,
                                                     wgpu::TextureFormat format,
                                                     std::vector<uint8_t> textureValues) {
        DoSamplingTestImpl(aspect,
                           CreateSamplingComputePipeline({TestAspect::Stencil}, {0, 1, 2, 3}),
                           format, textureValues, 4, [&](uint32_t expected, wgpu::Buffer buffer) {
                               EXPECT_BUFFER(buffer, 0, 4 * sizeof(uint32_t),
                                             new ExtraStencilComponentsExpectation(expected));
                           });
    }

    static bool CompareFunctionPasses(float compareRef,
                                      wgpu::CompareFunction compare,
                                      float textureValue) {
        switch (compare) {
            case wgpu::CompareFunction::Never:
                return false;
            case wgpu::CompareFunction::Less:
                return compareRef < textureValue;
            case wgpu::CompareFunction::LessEqual:
                return compareRef <= textureValue;
            case wgpu::CompareFunction::Greater:
                return compareRef > textureValue;
            case wgpu::CompareFunction::GreaterEqual:
                return compareRef >= textureValue;
            case wgpu::CompareFunction::Equal:
                return compareRef == textureValue;
            case wgpu::CompareFunction::NotEqual:
                return compareRef != textureValue;
            case wgpu::CompareFunction::Always:
                return true;
            default:
                return false;
        }
    }

    void DoDepthCompareRefTest(wgpu::RenderPipeline pipeline,
                               wgpu::TextureFormat format,
                               float compareRef,
                               wgpu::CompareFunction compare,
                               std::vector<float> textureValues) {
        queue.WriteBuffer(mUniformBuffer, 0, &compareRef, sizeof(float));

        wgpu::SamplerDescriptor samplerDesc;
        samplerDesc.compare = compare;
        wgpu::Sampler sampler = device.CreateSampler(&samplerDesc);

        wgpu::Texture inputTexture = CreateInputTexture(format);
        wgpu::TextureViewDescriptor inputViewDesc = {};
        inputViewDesc.aspect = wgpu::TextureAspect::DepthOnly;

        wgpu::BindGroup bindGroup =
            utils::MakeBindGroup(device, pipeline.GetBindGroupLayout(0),
                                 {
                                     {0, sampler},
                                     {1, inputTexture.CreateView(&inputViewDesc)},
                                     {2, mUniformBuffer},
                                 });

        wgpu::Texture outputTexture = CreateOutputTexture(wgpu::TextureFormat::R32Float);
        for (float textureValue : textureValues) {
            // Set the input depth texture to the provided texture value
            wgpu::CommandEncoder commandEncoder = device.CreateCommandEncoder();
            UpdateInputDepth(commandEncoder, inputTexture, textureValue);

            // Render into the output texture
            {
                utils::ComboRenderPassDescriptor passDescriptor({outputTexture.CreateView()});
                wgpu::RenderPassEncoder pass = commandEncoder.BeginRenderPass(&passDescriptor);
                pass.SetPipeline(pipeline);
                pass.SetBindGroup(0, bindGroup);
                pass.Draw(1);
                pass.EndPass();
            }

            wgpu::CommandBuffer commands = commandEncoder.Finish();
            queue.Submit(1, &commands);

            EXPECT_TEXTURE_EQ(CompareFunctionPasses(compareRef, compare, textureValue) ? 1.f : 0.f,
                              outputTexture, {0, 0});
        }
    }

    void DoDepthCompareRefTest(wgpu::ComputePipeline pipeline,
                               wgpu::TextureFormat format,
                               float compareRef,
                               wgpu::CompareFunction compare,
                               std::vector<float> textureValues) {
        queue.WriteBuffer(mUniformBuffer, 0, &compareRef, sizeof(float));

        wgpu::SamplerDescriptor samplerDesc;
        samplerDesc.compare = compare;
        wgpu::Sampler sampler = device.CreateSampler(&samplerDesc);

        wgpu::Texture inputTexture = CreateInputTexture(format);
        wgpu::TextureViewDescriptor inputViewDesc = {};
        inputViewDesc.aspect = wgpu::TextureAspect::DepthOnly;

        wgpu::Buffer outputBuffer = CreateOutputBuffer();

        wgpu::BindGroup bindGroup =
            utils::MakeBindGroup(device, pipeline.GetBindGroupLayout(0),
                                 {{0, sampler},
                                  {1, inputTexture.CreateView(&inputViewDesc)},
                                  {2, mUniformBuffer},
                                  {3, outputBuffer}});

        for (float textureValue : textureValues) {
            // Set the input depth texture to the provided texture value
            wgpu::CommandEncoder commandEncoder = device.CreateCommandEncoder();
            UpdateInputDepth(commandEncoder, inputTexture, textureValue);

            // Sample into the output buffer
            {
                wgpu::ComputePassEncoder pass = commandEncoder.BeginComputePass();
                pass.SetPipeline(pipeline);
                pass.SetBindGroup(0, bindGroup);
                pass.Dispatch(1);
                pass.EndPass();
            }

            wgpu::CommandBuffer commands = commandEncoder.Finish();
            queue.Submit(1, &commands);

            float float0 = 0.f;
            float float1 = 1.f;
            float* expected =
                CompareFunctionPasses(compareRef, compare, textureValue) ? &float1 : &float0;

            EXPECT_BUFFER_U32_EQ(*reinterpret_cast<uint32_t*>(expected), outputBuffer, 0);
        }
    }

  private:
    wgpu::Buffer mUniformBuffer;
};

// Test that sampling a depth texture with a render/compute pipeline works
TEST_P(DepthStencilSamplingTest, SampleDepth) {
    for (wgpu::TextureFormat format : kDepthFormats) {
        float tolerance = 0.0f;
        if (format == wgpu::TextureFormat::Depth16Unorm) {
            tolerance = 0.001f;
        }
        // Test 0, between [0, 1], and 1.
        DoSamplingTest(TestAspect::Depth, CreateSamplingRenderPipeline({TestAspect::Depth}, 0),
                       format, kNormalizedTextureValues, tolerance);

        DoSamplingTest(TestAspect::Depth, CreateSamplingComputePipeline({TestAspect::Depth}, 0),
                       format, kNormalizedTextureValues, tolerance);
    }
}

// Test that sampling a stencil texture with a render/compute pipeline works
TEST_P(DepthStencilSamplingTest, SampleStencil) {
    // TODO(crbug.com/dawn/593): This test requires glTextureView, which is unsupported on GLES.
    DAWN_TEST_UNSUPPORTED_IF(IsOpenGLES());
    for (wgpu::TextureFormat format : kStencilFormats) {
        DoSamplingTest(TestAspect::Stencil, CreateSamplingRenderPipeline({TestAspect::Stencil}, 0),
                       format, kStencilValues);

        DoSamplingTest(TestAspect::Stencil, CreateSamplingComputePipeline({TestAspect::Stencil}, 0),
                       format, kStencilValues);
    }
}

// Test that sampling a depth/stencil texture at components 1, 2, and 3 yield 0, 0, and 1
// respectively
TEST_P(DepthStencilSamplingTest, SampleExtraComponents) {
    // TODO(crbug.com/dawn/593): This test requires glTextureView, which is unsupported on GLES.
    DAWN_TEST_UNSUPPORTED_IF(IsOpenGLES());

    DoSamplingExtraStencilComponentsRenderTest(
        TestAspect::Stencil, wgpu::TextureFormat::Depth24PlusStencil8, {uint8_t(42), uint8_t(37)});

    DoSamplingExtraStencilComponentsComputeTest(
        TestAspect::Stencil, wgpu::TextureFormat::Depth24PlusStencil8, {uint8_t(42), uint8_t(37)});
}

// Test sampling both depth and stencil with a render/compute pipeline works.
TEST_P(DepthStencilSamplingTest, SampleDepthAndStencilRender) {
    // TODO(crbug.com/dawn/593): This test requires glTextureView, which is unsupported on GLES.
    DAWN_TEST_UNSUPPORTED_IF(IsOpenGLES());
    wgpu::SamplerDescriptor samplerDesc;
    wgpu::Sampler sampler = device.CreateSampler(&samplerDesc);

    wgpu::Texture inputTexture = CreateInputTexture(wgpu::TextureFormat::Depth24PlusStencil8);

    wgpu::TextureViewDescriptor depthViewDesc = {};
    depthViewDesc.aspect = wgpu::TextureAspect::DepthOnly;

    wgpu::TextureViewDescriptor stencilViewDesc = {};
    stencilViewDesc.aspect = wgpu::TextureAspect::StencilOnly;

    // With render pipeline
    {
        wgpu::RenderPipeline pipeline =
            CreateSamplingRenderPipeline({TestAspect::Depth, TestAspect::Stencil}, 0);

        wgpu::Buffer depthOutput = CreateOutputBuffer();
        wgpu::Buffer stencilOutput = CreateOutputBuffer();

        wgpu::BindGroup bindGroup =
            utils::MakeBindGroup(device, pipeline.GetBindGroupLayout(0),
                                 {
                                     {0, inputTexture.CreateView(&depthViewDesc)},
                                     {1, depthOutput},
                                     {2, inputTexture.CreateView(&stencilViewDesc)},
                                     {3, stencilOutput},
                                 });

        wgpu::CommandEncoder commandEncoder = device.CreateCommandEncoder();

        // Initialize both depth and stencil aspects.
        utils::ComboRenderPassDescriptor passDescriptor({}, inputTexture.CreateView());
        passDescriptor.cDepthStencilAttachmentInfo.clearDepth = 0.43f;
        passDescriptor.cDepthStencilAttachmentInfo.clearStencil = 31;

        wgpu::RenderPassEncoder pass = commandEncoder.BeginRenderPass(&passDescriptor);
        pass.EndPass();

        // Render into the output textures
        {
            utils::BasicRenderPass renderPass =
                utils::CreateBasicRenderPass(device, 1, 1, wgpu::TextureFormat::RGBA8Unorm);
            wgpu::RenderPassEncoder pass =
                commandEncoder.BeginRenderPass(&renderPass.renderPassInfo);
            pass.SetPipeline(pipeline);
            pass.SetBindGroup(0, bindGroup);
            pass.Draw(1);
            pass.EndPass();
        }

        wgpu::CommandBuffer commands = commandEncoder.Finish();
        queue.Submit(1, &commands);

        uint32_t expectedValueU32 = 0;
        memcpy(&expectedValueU32, &passDescriptor.cDepthStencilAttachmentInfo.clearDepth,
               sizeof(float));
        EXPECT_BUFFER_U32_EQ(expectedValueU32, depthOutput, 0);

        expectedValueU32 = 0;
        memcpy(&expectedValueU32, &passDescriptor.cDepthStencilAttachmentInfo.clearStencil,
               sizeof(uint8_t));
        EXPECT_BUFFER_U32_EQ(expectedValueU32, stencilOutput, 0);
    }

    // With compute pipeline
    {
        wgpu::ComputePipeline pipeline =
            CreateSamplingComputePipeline({TestAspect::Depth, TestAspect::Stencil}, 0);

        wgpu::Buffer depthOutput = CreateOutputBuffer();
        wgpu::Buffer stencilOutput = CreateOutputBuffer();

        wgpu::BindGroup bindGroup =
            utils::MakeBindGroup(device, pipeline.GetBindGroupLayout(0),
                                 {{0, inputTexture.CreateView(&depthViewDesc)},
                                  {1, depthOutput},
                                  {2, inputTexture.CreateView(&stencilViewDesc)},
                                  {3, stencilOutput}});

        wgpu::CommandEncoder commandEncoder = device.CreateCommandEncoder();
        // Initialize both depth and stencil aspects.
        utils::ComboRenderPassDescriptor passDescriptor({}, inputTexture.CreateView());
        passDescriptor.cDepthStencilAttachmentInfo.clearDepth = 0.43f;
        passDescriptor.cDepthStencilAttachmentInfo.clearStencil = 31;

        wgpu::RenderPassEncoder pass = commandEncoder.BeginRenderPass(&passDescriptor);
        pass.EndPass();

        // Sample into the output buffers
        {
            wgpu::ComputePassEncoder pass = commandEncoder.BeginComputePass();
            pass.SetPipeline(pipeline);
            pass.SetBindGroup(0, bindGroup);
            pass.Dispatch(1);
            pass.EndPass();
        }

        wgpu::CommandBuffer commands = commandEncoder.Finish();
        queue.Submit(1, &commands);

        uint32_t expectedValueU32 = 0;
        memcpy(&expectedValueU32, &passDescriptor.cDepthStencilAttachmentInfo.clearDepth,
               sizeof(float));
        EXPECT_BUFFER_U32_EQ(expectedValueU32, depthOutput, 0);

        expectedValueU32 = 0;
        memcpy(&expectedValueU32, &passDescriptor.cDepthStencilAttachmentInfo.clearStencil,
               sizeof(uint8_t));
        EXPECT_BUFFER_U32_EQ(expectedValueU32, stencilOutput, 0);
    }
}

// Test that sampling in a render pipeline with all of the compare functions works.
TEST_P(DepthStencilSamplingTest, CompareFunctionsRender) {
    // Initialization via renderPass loadOp doesn't work on Mac Intel.
    DAWN_SUPPRESS_TEST_IF(IsMetal() && IsIntel());

    wgpu::RenderPipeline pipeline = CreateComparisonRenderPipeline();

    for (wgpu::TextureFormat format : kDepthFormats) {
        // Test does not account for precision issues when comparison testing Depth16Unorm.
        if (format == wgpu::TextureFormat::Depth16Unorm) {
            continue;
        }

        // Test a "normal" ref value between 0 and 1; as well as negative and > 1 refs.
        for (float compareRef : kCompareRefs) {
            // Test 0, below the ref, equal to, above the ref, and 1.
            for (wgpu::CompareFunction f : kCompareFunctions) {
                DoDepthCompareRefTest(pipeline, format, compareRef, f, kNormalizedTextureValues);
            }
        }
    }
}

DAWN_INSTANTIATE_TEST(DepthStencilSamplingTest,
                      D3D12Backend(),
                      MetalBackend(),
                      OpenGLBackend(),
                      OpenGLESBackend(),
                      VulkanBackend());
