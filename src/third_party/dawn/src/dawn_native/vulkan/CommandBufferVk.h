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

#ifndef DAWNNATIVE_VULKAN_COMMANDBUFFERVK_H_
#define DAWNNATIVE_VULKAN_COMMANDBUFFERVK_H_

#include "dawn_native/CommandAllocator.h"
#include "dawn_native/CommandBuffer.h"

#include "common/vulkan_platform.h"

namespace dawn_native {
    struct BeginRenderPassCmd;
}  // namespace dawn_native

namespace dawn_native { namespace vulkan {

    class Device;

    class CommandBuffer : public CommandBufferBase {
      public:
        CommandBuffer(CommandEncoderBase* encoder, const CommandBufferDescriptor* descriptor);
        ~CommandBuffer();

        void RecordCommands(VkCommandBuffer commands);

      private:
        void RecordComputePass(VkCommandBuffer commands);
        void RecordRenderPass(VkCommandBuffer commands, BeginRenderPassCmd* renderPass);

        CommandIterator mCommands;
    };

}}  // namespace dawn_native::vulkan

#endif  // DAWNNATIVE_VULKAN_COMMANDBUFFERVK_H_
