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

#include "dawn_native/PassResourceUsageTracker.h"

#include "dawn_native/BindGroup.h"
#include "dawn_native/Buffer.h"
#include "dawn_native/EnumMaskIterator.h"
#include "dawn_native/ExternalTexture.h"
#include "dawn_native/Format.h"
#include "dawn_native/QuerySet.h"
#include "dawn_native/Texture.h"

#include <utility>

namespace dawn_native {

    void SyncScopeUsageTracker::BufferUsedAs(BufferBase* buffer, wgpu::BufferUsage usage) {
        // std::map's operator[] will create the key and return 0 if the key didn't exist
        // before.
        mBufferUsages[buffer] |= usage;
    }

    void SyncScopeUsageTracker::TextureViewUsedAs(TextureViewBase* view, wgpu::TextureUsage usage) {
        TextureBase* texture = view->GetTexture();
        const SubresourceRange& range = view->GetSubresourceRange();

        // Get or create a new TextureSubresourceUsage for that texture (initially filled with
        // wgpu::TextureUsage::None)
        auto it = mTextureUsages.emplace(
            std::piecewise_construct, std::forward_as_tuple(texture),
            std::forward_as_tuple(texture->GetFormat().aspects, texture->GetArrayLayers(),
                                  texture->GetNumMipLevels(), wgpu::TextureUsage::None));
        TextureSubresourceUsage& textureUsage = it.first->second;

        textureUsage.Update(range,
                            [usage](const SubresourceRange&, wgpu::TextureUsage* storedUsage) {
                                // TODO(crbug.com/dawn/1001): Consider optimizing to have fewer
                                // branches.
                                if ((*storedUsage & wgpu::TextureUsage::RenderAttachment) != 0 &&
                                    (usage & wgpu::TextureUsage::RenderAttachment) != 0) {
                                    // Using the same subresource as an attachment for two different
                                    // render attachments is a write-write hazard. Add this internal
                                    // usage so we will fail the check that a subresource with
                                    // writable usage is the single usage.
                                    *storedUsage |= kAgainAsRenderAttachment;
                                }
                                *storedUsage |= usage;
                            });
    }

    void SyncScopeUsageTracker::AddRenderBundleTextureUsage(
        TextureBase* texture,
        const TextureSubresourceUsage& textureUsage) {
        // Get or create a new TextureSubresourceUsage for that texture (initially filled with
        // wgpu::TextureUsage::None)
        auto it = mTextureUsages.emplace(
            std::piecewise_construct, std::forward_as_tuple(texture),
            std::forward_as_tuple(texture->GetFormat().aspects, texture->GetArrayLayers(),
                                  texture->GetNumMipLevels(), wgpu::TextureUsage::None));
        TextureSubresourceUsage* passTextureUsage = &it.first->second;

        passTextureUsage->Merge(
            textureUsage, [](const SubresourceRange&, wgpu::TextureUsage* storedUsage,
                             const wgpu::TextureUsage& addedUsage) {
                ASSERT((addedUsage & wgpu::TextureUsage::RenderAttachment) == 0);
                *storedUsage |= addedUsage;
            });
    }

    void SyncScopeUsageTracker::AddBindGroup(BindGroupBase* group) {
        for (BindingIndex bindingIndex{0}; bindingIndex < group->GetLayout()->GetBindingCount();
             ++bindingIndex) {
            const BindingInfo& bindingInfo = group->GetLayout()->GetBindingInfo(bindingIndex);

            switch (bindingInfo.bindingType) {
                case BindingInfoType::Buffer: {
                    BufferBase* buffer = group->GetBindingAsBufferBinding(bindingIndex).buffer;
                    switch (bindingInfo.buffer.type) {
                        case wgpu::BufferBindingType::Uniform:
                            BufferUsedAs(buffer, wgpu::BufferUsage::Uniform);
                            break;
                        case wgpu::BufferBindingType::Storage:
                            BufferUsedAs(buffer, wgpu::BufferUsage::Storage);
                            break;
                        case kInternalStorageBufferBinding:
                            BufferUsedAs(buffer, kInternalStorageBuffer);
                            break;
                        case wgpu::BufferBindingType::ReadOnlyStorage:
                            BufferUsedAs(buffer, kReadOnlyStorageBuffer);
                            break;
                        case wgpu::BufferBindingType::Undefined:
                            UNREACHABLE();
                    }
                    break;
                }

                case BindingInfoType::Texture: {
                    TextureViewBase* view = group->GetBindingAsTextureView(bindingIndex);
                    TextureViewUsedAs(view, wgpu::TextureUsage::TextureBinding);
                    break;
                }

                case BindingInfoType::StorageTexture: {
                    TextureViewBase* view = group->GetBindingAsTextureView(bindingIndex);
                    switch (bindingInfo.storageTexture.access) {
                        case wgpu::StorageTextureAccess::WriteOnly:
                            TextureViewUsedAs(view, wgpu::TextureUsage::StorageBinding);
                            break;
                        case wgpu::StorageTextureAccess::Undefined:
                            UNREACHABLE();
                    }
                    break;
                }

                case BindingInfoType::ExternalTexture: {
                    ExternalTextureBase* externalTexture =
                        group->GetBindingAsExternalTexture(bindingIndex);

                    const std::array<Ref<TextureViewBase>, kMaxPlanesPerFormat>& textureViews =
                        externalTexture->GetTextureViews();

                    // Only single-plane formats are supported right now, so assert only one
                    // view exists.
                    ASSERT(textureViews[1].Get() == nullptr);
                    ASSERT(textureViews[2].Get() == nullptr);

                    mExternalTextureUsages.insert(externalTexture);
                    TextureViewUsedAs(textureViews[0].Get(), wgpu::TextureUsage::TextureBinding);
                    break;
                }

                case BindingInfoType::Sampler:
                    break;
            }
        }
    }

    SyncScopeResourceUsage SyncScopeUsageTracker::AcquireSyncScopeUsage() {
        SyncScopeResourceUsage result;
        result.buffers.reserve(mBufferUsages.size());
        result.bufferUsages.reserve(mBufferUsages.size());
        result.textures.reserve(mTextureUsages.size());
        result.textureUsages.reserve(mTextureUsages.size());

        for (auto& it : mBufferUsages) {
            result.buffers.push_back(it.first);
            result.bufferUsages.push_back(it.second);
        }

        for (auto& it : mTextureUsages) {
            result.textures.push_back(it.first);
            result.textureUsages.push_back(std::move(it.second));
        }

        for (auto& it : mExternalTextureUsages) {
            result.externalTextures.push_back(it);
        }

        mBufferUsages.clear();
        mTextureUsages.clear();
        mExternalTextureUsages.clear();

        return result;
    }

    void ComputePassResourceUsageTracker::AddDispatch(SyncScopeResourceUsage scope) {
        mUsage.dispatchUsages.push_back(std::move(scope));
    }

    void ComputePassResourceUsageTracker::AddReferencedBuffer(BufferBase* buffer) {
        mUsage.referencedBuffers.insert(buffer);
    }

    void ComputePassResourceUsageTracker::AddResourcesReferencedByBindGroup(BindGroupBase* group) {
        for (BindingIndex index{0}; index < group->GetLayout()->GetBindingCount(); ++index) {
            const BindingInfo& bindingInfo = group->GetLayout()->GetBindingInfo(index);

            switch (bindingInfo.bindingType) {
                case BindingInfoType::Buffer: {
                    mUsage.referencedBuffers.insert(group->GetBindingAsBufferBinding(index).buffer);
                    break;
                }

                case BindingInfoType::Texture: {
                    mUsage.referencedTextures.insert(
                        group->GetBindingAsTextureView(index)->GetTexture());
                    break;
                }

                case BindingInfoType::ExternalTexture: {
                    ExternalTextureBase* externalTexture =
                        group->GetBindingAsExternalTexture(index);
                    const std::array<Ref<TextureViewBase>, kMaxPlanesPerFormat>& textureViews =
                        externalTexture->GetTextureViews();

                    // Only single-plane formats are supported right now, so assert only one
                    // view exists.
                    ASSERT(textureViews[1].Get() == nullptr);
                    ASSERT(textureViews[2].Get() == nullptr);

                    mUsage.referencedExternalTextures.insert(externalTexture);
                    mUsage.referencedTextures.insert(textureViews[0].Get()->GetTexture());
                    break;
                }

                case BindingInfoType::StorageTexture:
                case BindingInfoType::Sampler:
                    break;
            }
        }
    }

    ComputePassResourceUsage ComputePassResourceUsageTracker::AcquireResourceUsage() {
        return std::move(mUsage);
    }

    RenderPassResourceUsage RenderPassResourceUsageTracker::AcquireResourceUsage() {
        RenderPassResourceUsage result;
        *static_cast<SyncScopeResourceUsage*>(&result) = AcquireSyncScopeUsage();

        result.querySets.reserve(mQueryAvailabilities.size());
        result.queryAvailabilities.reserve(mQueryAvailabilities.size());

        for (auto& it : mQueryAvailabilities) {
            result.querySets.push_back(it.first);
            result.queryAvailabilities.push_back(std::move(it.second));
        }

        mQueryAvailabilities.clear();

        return result;
    }

    void RenderPassResourceUsageTracker::TrackQueryAvailability(QuerySetBase* querySet,
                                                                uint32_t queryIndex) {
        // The query availability only needs to be tracked again on render passes for checking
        // query overwrite on render pass and resetting query sets on the Vulkan backend.
        DAWN_ASSERT(querySet != nullptr);

        // Gets the iterator for that querySet or create a new vector of bool set to false
        // if the querySet wasn't registered.
        auto it = mQueryAvailabilities.emplace(querySet, querySet->GetQueryCount()).first;
        it->second[queryIndex] = true;
    }

    const QueryAvailabilityMap& RenderPassResourceUsageTracker::GetQueryAvailabilityMap() const {
        return mQueryAvailabilities;
    }

}  // namespace dawn_native
