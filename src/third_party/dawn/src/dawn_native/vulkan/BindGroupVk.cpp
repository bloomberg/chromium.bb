// Copyright 2018 The Dawn Authors
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

#include "dawn_native/vulkan/BindGroupVk.h"

#include "common/BitSetIterator.h"
#include "dawn_native/vulkan/BindGroupLayoutVk.h"
#include "dawn_native/vulkan/BufferVk.h"
#include "dawn_native/vulkan/DeviceVk.h"
#include "dawn_native/vulkan/FencedDeleter.h"
#include "dawn_native/vulkan/SamplerVk.h"
#include "dawn_native/vulkan/TextureVk.h"
#include "dawn_native/vulkan/VulkanError.h"

namespace dawn_native { namespace vulkan {

    // static
    ResultOrError<BindGroup*> BindGroup::Create(Device* device,
                                                const BindGroupDescriptor* descriptor) {
        std::unique_ptr<BindGroup> group = std::make_unique<BindGroup>(device, descriptor);
        DAWN_TRY(group->Initialize());
        return group.release();
    }

    MaybeError BindGroup::Initialize() {
        Device* device = ToBackend(GetDevice());

        DAWN_TRY_ASSIGN(mAllocation, ToBackend(GetLayout())->AllocateOneSet());

        // Now do a write of a single descriptor set with all possible chained data allocated on the
        // stack.
        uint32_t numWrites = 0;
        std::array<VkWriteDescriptorSet, kMaxBindingsPerGroup> writes;
        std::array<VkDescriptorBufferInfo, kMaxBindingsPerGroup> writeBufferInfo;
        std::array<VkDescriptorImageInfo, kMaxBindingsPerGroup> writeImageInfo;

        const auto& layoutInfo = GetLayout()->GetBindingInfo();
        for (uint32_t bindingIndex : IterateBitSet(layoutInfo.mask)) {
            auto& write = writes[numWrites];
            write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            write.pNext = nullptr;
            write.dstSet = mAllocation.set;
            write.dstBinding = bindingIndex;
            write.dstArrayElement = 0;
            write.descriptorCount = 1;
            write.descriptorType = VulkanDescriptorType(layoutInfo.types[bindingIndex],
                                                        layoutInfo.hasDynamicOffset[bindingIndex]);

            switch (layoutInfo.types[bindingIndex]) {
                case wgpu::BindingType::UniformBuffer:
                case wgpu::BindingType::StorageBuffer: {
                    BufferBinding binding = GetBindingAsBufferBinding(bindingIndex);

                    writeBufferInfo[numWrites].buffer = ToBackend(binding.buffer)->GetHandle();
                    writeBufferInfo[numWrites].offset = binding.offset;
                    writeBufferInfo[numWrites].range = binding.size;
                    write.pBufferInfo = &writeBufferInfo[numWrites];
                } break;

                case wgpu::BindingType::Sampler: {
                    Sampler* sampler = ToBackend(GetBindingAsSampler(bindingIndex));
                    writeImageInfo[numWrites].sampler = sampler->GetHandle();
                    write.pImageInfo = &writeImageInfo[numWrites];
                } break;

                case wgpu::BindingType::SampledTexture: {
                    TextureView* view = ToBackend(GetBindingAsTextureView(bindingIndex));

                    writeImageInfo[numWrites].imageView = view->GetHandle();
                    // TODO(cwallez@chromium.org): This isn't true in general: if the image has
                    // two read-only usages one of which is Sampled. Works for now though :)
                    writeImageInfo[numWrites].imageLayout =
                        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

                    write.pImageInfo = &writeImageInfo[numWrites];
                } break;

                default:
                    UNREACHABLE();
            }

            numWrites++;
        }

        // TODO(cwallez@chromium.org): Batch these updates
        device->fn.UpdateDescriptorSets(device->GetVkDevice(), numWrites, writes.data(), 0,
                                        nullptr);

        return {};
    }

    BindGroup::~BindGroup() {
        ToBackend(GetLayout())->Deallocate(&mAllocation);
    }

    VkDescriptorSet BindGroup::GetHandle() const {
        return mAllocation.set;
    }

}}  // namespace dawn_native::vulkan
