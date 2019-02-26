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

#include "SampleUtils.h"

#include "utils/DawnHelpers.h"
#include "utils/SystemUtils.h"

dawnDevice device;
dawnQueue queue;
dawnSwapChain swapchain;
dawnRenderPipeline pipeline;

dawnTextureFormat swapChainFormat;

void init() {
    device = CreateCppDawnDevice().Release();
    queue = dawnDeviceCreateQueue(device);

    {
        dawnSwapChainBuilder builder = dawnDeviceCreateSwapChainBuilder(device);
        uint64_t swapchainImpl = GetSwapChainImplementation();
        dawnSwapChainBuilderSetImplementation(builder, swapchainImpl);
        swapchain = dawnSwapChainBuilderGetResult(builder);
        dawnSwapChainBuilderRelease(builder);
    }
    swapChainFormat = static_cast<dawnTextureFormat>(GetPreferredSwapChainTextureFormat());
    dawnSwapChainConfigure(swapchain, swapChainFormat, DAWN_TEXTURE_USAGE_BIT_OUTPUT_ATTACHMENT, 640,
                          480);

    const char* vs =
        "#version 450\n"
        "const vec2 pos[3] = vec2[3](vec2(0.0f, 0.5f), vec2(-0.5f, -0.5f), vec2(0.5f, -0.5f));\n"
        "void main() {\n"
        "   gl_Position = vec4(pos[gl_VertexIndex], 0.0, 1.0);\n"
        "}\n";
    dawnShaderModule vsModule = utils::CreateShaderModule(dawn::Device(device), dawn::ShaderStage::Vertex, vs).Release();

    const char* fs =
        "#version 450\n"
        "layout(location = 0) out vec4 fragColor;"
        "void main() {\n"
        "   fragColor = vec4(1.0, 0.0, 0.0, 1.0);\n"
        "}\n";
    dawnShaderModule fsModule = utils::CreateShaderModule(device, dawn::ShaderStage::Fragment, fs).Release();

    {
        dawnRenderPipelineBuilder builder = dawnDeviceCreateRenderPipelineBuilder(device);
        dawnRenderPipelineBuilderSetColorAttachmentFormat(builder, 0, swapChainFormat);
        dawnRenderPipelineBuilderSetStage(builder, DAWN_SHADER_STAGE_VERTEX, vsModule, "main");
        dawnRenderPipelineBuilderSetStage(builder, DAWN_SHADER_STAGE_FRAGMENT, fsModule, "main");
        pipeline = dawnRenderPipelineBuilderGetResult(builder);
        dawnRenderPipelineBuilderRelease(builder);
    }

    dawnShaderModuleRelease(vsModule);
    dawnShaderModuleRelease(fsModule);
}

void frame() {
    dawnTexture backbuffer = dawnSwapChainGetNextTexture(swapchain);
    dawnTextureView backbufferView;
    {
        backbufferView = dawnTextureCreateDefaultTextureView(backbuffer);
    }
    dawnRenderPassDescriptor renderpassInfo;
    {
        dawnRenderPassDescriptorBuilder builder = dawnDeviceCreateRenderPassDescriptorBuilder(device);
        dawnRenderPassDescriptorBuilderSetColorAttachment(builder, 0, backbufferView, DAWN_LOAD_OP_CLEAR);
        renderpassInfo = dawnRenderPassDescriptorBuilderGetResult(builder);
        dawnRenderPassDescriptorBuilderRelease(builder);
    }
    dawnCommandBuffer commands;
    {
        dawnCommandBufferBuilder builder = dawnDeviceCreateCommandBufferBuilder(device);

        dawnRenderPassEncoder pass = dawnCommandBufferBuilderBeginRenderPass(builder, renderpassInfo);
        dawnRenderPassEncoderSetRenderPipeline(pass, pipeline);
        dawnRenderPassEncoderDrawArrays(pass, 3, 1, 0, 0);
        dawnRenderPassEncoderEndPass(pass);
        dawnRenderPassEncoderRelease(pass);

        commands = dawnCommandBufferBuilderGetResult(builder);
        dawnCommandBufferBuilderRelease(builder);
    }

    dawnQueueSubmit(queue, 1, &commands);
    dawnCommandBufferRelease(commands);
    dawnSwapChainPresent(swapchain, backbuffer);
    dawnRenderPassDescriptorRelease(renderpassInfo);
    dawnTextureViewRelease(backbufferView);

    DoFlush();
}

int main(int argc, const char* argv[]) {
    if (!InitSample(argc, argv)) {
        return 1;
    }
    init();

    while (!ShouldQuit()) {
        frame();
        utils::USleep(16000);
    }

    // TODO release stuff
}
