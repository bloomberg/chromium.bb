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
#include "common/ityp_stack_vec.h"
#include "dawn_native/vulkan/BindGroupLayoutVk.h"
#include "dawn_native/vulkan/BufferVk.h"
#include "dawn_native/vulkan/DeviceVk.h"
#include "dawn_native/vulkan/FencedDeleter.h"
#include "dawn_native/vulkan/SamplerVk.h"
#include "dawn_native/vulkan/TextureVk.h"
#include "dawn_native/vulkan/VulkanError.h"

namespace dawn_native { namespace vulkan {

    // static
    ResultOrError<Ref<BindGroup>> BindGroup::Create(Device* device,
                                                    const BindGroupDescriptor* descriptor) {
        return ToBackend(descriptor->layout)->AllocateBindGroup(device, descriptor);
    }

    BindGroup::BindGroup(Device* device,
                         const BindGroupDescriptor* descriptor,
                         DescriptorSetAllocation descriptorSetAllocation)
        : BindGroupBase(this, device, descriptor),
          mDescriptorSetAllocation(descriptorSetAllocation) {
        // Now do a write of a single descriptor set with all possible chained data allocated on the
        // stack.
        const uint32_t bindingCount = static_cast<uint32_t>((GetLayout()->GetBindingCount()));
        ityp::stack_vec<uint32_t, VkWriteDescriptorSet, kMaxOptimalBindingsPerGroup> writes(
            bindingCount);
        ityp::stack_vec<uint32_t, VkDescriptorBufferInfo, kMaxOptimalBindingsPerGroup>
            writeBufferInfo(bindingCount);
        ityp::stack_vec<uint32_t, VkDescriptorImageInfo, kMaxOptimalBindingsPerGroup>
            writeImageInfo(bindingCount);

        bool useBindingIndex = device->IsToggleEnabled(Toggle::UseTintGenerator);

        uint32_t numWrites = 0;
        for (const auto& it : GetLayout()->GetBindingMap()) {
            BindingNumber bindingNumber = it.first;
            BindingIndex bindingIndex = it.second;
            const BindingInfo& bindingInfo = GetLayout()->GetBindingInfo(bindingIndex);

            auto& write = writes[numWrites];
            write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            write.pNext = nullptr;
            write.dstSet = GetHandle();
            write.dstBinding = useBindingIndex ? static_cast<uint32_t>(bindingIndex)
                                               : static_cast<uint32_t>(bindingNumber);
            write.dstArrayElement = 0;
            write.descriptorCount = 1;
            write.descriptorType = VulkanDescriptorType(bindingInfo);

            switch (bindingInfo.bindingType) {
                case BindingInfoType::Buffer: {
                    BufferBinding binding = GetBindingAsBufferBinding(bindingIndex);

                    VkBuffer handle = ToBackend(binding.buffer)->GetHandle();
                    if (handle == VK_NULL_HANDLE) {
                        // The Buffer was destroyed. Skip this descriptor write since it would be
                        // a Vulkan Validation Layers error. This bind group won't be used as it
                        // is an error to submit a command buffer that references destroyed
                        // resources.
                        continue;
                    }
                    writeBufferInfo[numWrites].buffer = handle;
                    writeBufferInfo[numWrites].offset = binding.offset;
                    writeBufferInfo[numWrites].range = binding.size;
                    write.pBufferInfo = &writeBufferInfo[numWrites];
                    break;
                }

                case BindingInfoType::Sampler: {
                    Sampler* sampler = ToBackend(GetBindingAsSampler(bindingIndex));
                    writeImageInfo[numWrites].sampler = sampler->GetHandle();
                    write.pImageInfo = &writeImageInfo[numWrites];
                    break;
                }

                case BindingInfoType::Texture: {
                    TextureView* view = ToBackend(GetBindingAsTextureView(bindingIndex));

                    writeImageInfo[numWrites].imageView = view->GetHandle();
                    // The layout may be GENERAL here because of interactions between the Sampled
                    // and ReadOnlyStorage usages. See the logic in VulkanImageLayout.
                    writeImageInfo[numWrites].imageLayout = VulkanImageLayout(
                        ToBackend(view->GetTexture()), wgpu::TextureUsage::Sampled);

                    write.pImageInfo = &writeImageInfo[numWrites];
                    break;
                }

                case BindingInfoType::StorageTexture: {
                    TextureView* view = ToBackend(GetBindingAsTextureView(bindingIndex));

                    writeImageInfo[numWrites].imageView = view->GetHandle();
                    writeImageInfo[numWrites].imageLayout = VK_IMAGE_LAYOUT_GENERAL;

                    write.pImageInfo = &writeImageInfo[numWrites];
                    break;
                }
            }

            numWrites++;
        }

        // TODO(cwallez@chromium.org): Batch these updates
        device->fn.UpdateDescriptorSets(device->GetVkDevice(), numWrites, writes.data(), 0,
                                        nullptr);
    }

    BindGroup::~BindGroup() {
        ToBackend(GetLayout())->DeallocateBindGroup(this, &mDescriptorSetAllocation);
    }

    VkDescriptorSet BindGroup::GetHandle() const {
        return mDescriptorSetAllocation.set;
    }

}}  // namespace dawn_native::vulkan
