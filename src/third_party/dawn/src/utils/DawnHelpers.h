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

#include <dawn/dawncpp.h>

#include <initializer_list>

namespace utils {

    enum Expectation { Success, Failure };

    dawn::ShaderModule CreateShaderModule(const dawn::Device& device,
                                          dawn::ShaderStage stage,
                                          const char* source);
    dawn::ShaderModule CreateShaderModuleFromASM(const dawn::Device& device, const char* source);

    dawn::Buffer CreateBufferFromData(const dawn::Device& device,
                                      const void* data,
                                      uint32_t size,
                                      dawn::BufferUsageBit usage);

    template <typename T>
    dawn::Buffer CreateBufferFromData(const dawn::Device& device,
                                      dawn::BufferUsageBit usage,
                                      std::initializer_list<T> data) {
        return CreateBufferFromData(device, data.begin(), uint32_t(sizeof(T) * data.size()), usage);
    }

    dawn::BufferCopyView CreateBufferCopyView(dawn::Buffer buffer,
                                              uint32_t offset,
                                              uint32_t rowPitch,
                                              uint32_t imageHeight);
    dawn::TextureCopyView CreateTextureCopyView(dawn::Texture texture,
                                                uint32_t level,
                                                uint32_t slice,
                                                dawn::Origin3D origin,
                                                dawn::TextureAspect aspect);

    struct BasicRenderPass {
        uint32_t width;
        uint32_t height;
        dawn::Texture color;
        dawn::TextureFormat colorFormat;
        dawn::RenderPassDescriptor renderPassInfo;
    };
    BasicRenderPass CreateBasicRenderPass(const dawn::Device& device,
                                          uint32_t width,
                                          uint32_t height);

    dawn::SamplerDescriptor GetDefaultSamplerDescriptor();
    dawn::PipelineLayout MakeBasicPipelineLayout(const dawn::Device& device,
                                                 const dawn::BindGroupLayout* bindGroupLayout);
    dawn::BindGroupLayout MakeBindGroupLayout(
        const dawn::Device& device,
        std::initializer_list<dawn::BindGroupBinding> bindingsInitializer);

}  // namespace utils
