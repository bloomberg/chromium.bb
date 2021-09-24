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

#ifndef DAWNNATIVE_D3D12_TEXTURED3D12_H_
#define DAWNNATIVE_D3D12_TEXTURED3D12_H_

#include "dawn_native/Texture.h"

#include "dawn_native/DawnNative.h"
#include "dawn_native/IntegerTypes.h"
#include "dawn_native/PassResourceUsage.h"
#include "dawn_native/d3d12/IntegerTypes.h"
#include "dawn_native/d3d12/ResourceHeapAllocationD3D12.h"
#include "dawn_native/d3d12/d3d12_platform.h"

namespace dawn_native { namespace d3d12 {

    class CommandRecordingContext;
    class Device;
    class D3D11on12ResourceCacheEntry;

    DXGI_FORMAT D3D12TextureFormat(wgpu::TextureFormat format);
    MaybeError ValidateD3D12TextureCanBeWrapped(ID3D12Resource* d3d12Resource,
                                                const TextureDescriptor* descriptor);
    MaybeError ValidateTextureDescriptorCanBeWrapped(const TextureDescriptor* descriptor);
    MaybeError ValidateD3D12VideoTextureCanBeShared(Device* device, DXGI_FORMAT textureFormat);

    class Texture final : public TextureBase {
      public:
        static ResultOrError<Ref<Texture>> Create(Device* device,
                                                  const TextureDescriptor* descriptor);
        static ResultOrError<Ref<Texture>> CreateExternalImage(
            Device* device,
            const TextureDescriptor* descriptor,
            ComPtr<ID3D12Resource> d3d12Texture,
            Ref<D3D11on12ResourceCacheEntry> d3d11on12Resource,
            ExternalMutexSerial acquireMutexKey,
            ExternalMutexSerial releaseMutexKey,
            bool isSwapChainTexture,
            bool isInitialized);
        static ResultOrError<Ref<Texture>> Create(Device* device,
                                                  const TextureDescriptor* descriptor,
                                                  ComPtr<ID3D12Resource> d3d12Texture);

        DXGI_FORMAT GetD3D12Format() const;
        ID3D12Resource* GetD3D12Resource() const;
        DXGI_FORMAT GetD3D12CopyableSubresourceFormat(Aspect aspect) const;

        D3D12_RENDER_TARGET_VIEW_DESC GetRTVDescriptor(uint32_t mipLevel,
                                                       uint32_t baseSlice,
                                                       uint32_t sliceCount) const;
        D3D12_DEPTH_STENCIL_VIEW_DESC GetDSVDescriptor(uint32_t mipLevel,
                                                       uint32_t baseArrayLayer,
                                                       uint32_t layerCount) const;
        void EnsureSubresourceContentInitialized(CommandRecordingContext* commandContext,
                                                 const SubresourceRange& range);

        void TrackUsageAndGetResourceBarrierForPass(CommandRecordingContext* commandContext,
                                                    std::vector<D3D12_RESOURCE_BARRIER>* barrier,
                                                    const TextureSubresourceUsage& textureUsages);
        void TransitionUsageAndGetResourceBarrier(CommandRecordingContext* commandContext,
                                                  std::vector<D3D12_RESOURCE_BARRIER>* barrier,
                                                  wgpu::TextureUsage usage,
                                                  const SubresourceRange& range);
        void TrackUsageAndTransitionNow(CommandRecordingContext* commandContext,
                                        wgpu::TextureUsage usage,
                                        const SubresourceRange& range);
        void TrackUsageAndTransitionNow(CommandRecordingContext* commandContext,
                                        D3D12_RESOURCE_STATES newState,
                                        const SubresourceRange& range);
        void TrackAllUsageAndTransitionNow(CommandRecordingContext* commandContext,
                                           wgpu::TextureUsage usage);
        void TrackAllUsageAndTransitionNow(CommandRecordingContext* commandContext,
                                           D3D12_RESOURCE_STATES newState);

      private:
        Texture(Device* device, const TextureDescriptor* descriptor, TextureState state);
        ~Texture() override;
        using TextureBase::TextureBase;

        MaybeError InitializeAsInternalTexture();
        MaybeError InitializeAsExternalTexture(const TextureDescriptor* descriptor,
                                               ComPtr<ID3D12Resource> d3d12Texture,
                                               Ref<D3D11on12ResourceCacheEntry> d3d11on12Resource,
                                               ExternalMutexSerial acquireMutexKey,
                                               ExternalMutexSerial releaseMutexKey,
                                               bool isSwapChainTexture);
        MaybeError InitializeAsSwapChainTexture(ComPtr<ID3D12Resource> d3d12Texture);

        void SetLabelHelper(const char* prefix);

        // Dawn API
        void SetLabelImpl() override;
        void DestroyImpl() override;

        MaybeError ClearTexture(CommandRecordingContext* commandContext,
                                const SubresourceRange& range,
                                TextureBase::ClearValue clearValue);

        // Barriers implementation details.
        struct StateAndDecay {
            D3D12_RESOURCE_STATES lastState;
            ExecutionSerial lastDecaySerial;
            bool isValidToDecay;

            bool operator==(const StateAndDecay& other) const;
        };
        void TransitionUsageAndGetResourceBarrier(CommandRecordingContext* commandContext,
                                                  std::vector<D3D12_RESOURCE_BARRIER>* barrier,
                                                  D3D12_RESOURCE_STATES newState,
                                                  const SubresourceRange& range);
        void TransitionSubresourceRange(std::vector<D3D12_RESOURCE_BARRIER>* barriers,
                                        const SubresourceRange& range,
                                        StateAndDecay* state,
                                        D3D12_RESOURCE_STATES subresourceNewState,
                                        ExecutionSerial pendingCommandSerial) const;
        void HandleTransitionSpecialCases(CommandRecordingContext* commandContext);

        SubresourceStorage<StateAndDecay> mSubresourceStateAndDecay;

        ResourceHeapAllocation mResourceAllocation;
        bool mSwapChainTexture = false;
        D3D12_RESOURCE_FLAGS mD3D12ResourceFlags;

        ExternalMutexSerial mAcquireMutexKey = ExternalMutexSerial(0);
        ExternalMutexSerial mReleaseMutexKey = ExternalMutexSerial(0);
        Ref<D3D11on12ResourceCacheEntry> mD3D11on12Resource;
    };

    class TextureView final : public TextureViewBase {
      public:
        static Ref<TextureView> Create(TextureBase* texture,
                                       const TextureViewDescriptor* descriptor);

        DXGI_FORMAT GetD3D12Format() const;

        const D3D12_SHADER_RESOURCE_VIEW_DESC& GetSRVDescriptor() const;
        D3D12_RENDER_TARGET_VIEW_DESC GetRTVDescriptor() const;
        D3D12_DEPTH_STENCIL_VIEW_DESC GetDSVDescriptor() const;
        D3D12_UNORDERED_ACCESS_VIEW_DESC GetUAVDescriptor() const;

      private:
        TextureView(TextureBase* texture, const TextureViewDescriptor* descriptor);

        D3D12_SHADER_RESOURCE_VIEW_DESC mSrvDesc;
    };
}}  // namespace dawn_native::d3d12

#endif  // DAWNNATIVE_D3D12_TEXTURED3D12_H_
