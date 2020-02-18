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

#ifndef DAWNNATIVE_DEVICE_H_
#define DAWNNATIVE_DEVICE_H_

#include "common/Serial.h"
#include "dawn_native/Error.h"
#include "dawn_native/Format.h"
#include "dawn_native/Forward.h"
#include "dawn_native/ObjectBase.h"
#include "dawn_native/Toggles.h"

#include "dawn_native/DawnNative.h"
#include "dawn_native/dawn_platform.h"

#include <memory>

namespace dawn_native {

    using ErrorCallback = void (*)(const char* errorMessage, void* userData);

    class AdapterBase;
    class FenceSignalTracker;
    class DynamicUploader;
    class StagingBufferBase;

    class DeviceBase {
      public:
        DeviceBase(AdapterBase* adapter, const DeviceDescriptor* descriptor);
        virtual ~DeviceBase();

        void HandleError(const char* message);

        bool ConsumedError(MaybeError maybeError) {
            if (DAWN_UNLIKELY(maybeError.IsError())) {
                ConsumeError(maybeError.AcquireError());
                return true;
            }
            return false;
        }

        MaybeError ValidateObject(const ObjectBase* object) const;

        AdapterBase* GetAdapter() const;

        FenceSignalTracker* GetFenceSignalTracker() const;

        // Returns the Format corresponding to the dawn::TextureFormat or an error if the format
        // isn't a valid dawn::TextureFormat or isn't supported by this device.
        // The pointer returned has the same lifetime as the device.
        ResultOrError<const Format*> GetInternalFormat(dawn::TextureFormat format) const;

        // Returns the Format corresponding to the dawn::TextureFormat and assumes the format is
        // valid and supported.
        // The reference returned has the same lifetime as the device.
        const Format& GetValidInternalFormat(dawn::TextureFormat format) const;

        virtual CommandBufferBase* CreateCommandBuffer(
            CommandEncoderBase* encoder,
            const CommandBufferDescriptor* descriptor) = 0;

        virtual Serial GetCompletedCommandSerial() const = 0;
        virtual Serial GetLastSubmittedCommandSerial() const = 0;
        virtual Serial GetPendingCommandSerial() const = 0;
        virtual void TickImpl() = 0;

        // Many Dawn objects are completely immutable once created which means that if two
        // creations are given the same arguments, they can return the same object. Reusing
        // objects will help make comparisons between objects by a single pointer comparison.
        //
        // Technically no object is immutable as they have a reference count, and an
        // application with reference-counting issues could "see" that objects are reused.
        // This is solved by automatic-reference counting, and also the fact that when using
        // the client-server wire every creation will get a different proxy object, with a
        // different reference count.
        //
        // When trying to create an object, we give both the descriptor and an example of what
        // the created object will be, the "blueprint". The blueprint is just a FooBase object
        // instead of a backend Foo object. If the blueprint doesn't match an object in the
        // cache, then the descriptor is used to make a new object.
        ResultOrError<BindGroupLayoutBase*> GetOrCreateBindGroupLayout(
            const BindGroupLayoutDescriptor* descriptor);
        void UncacheBindGroupLayout(BindGroupLayoutBase* obj);

        ResultOrError<ComputePipelineBase*> GetOrCreateComputePipeline(
            const ComputePipelineDescriptor* descriptor);
        void UncacheComputePipeline(ComputePipelineBase* obj);

        ResultOrError<PipelineLayoutBase*> GetOrCreatePipelineLayout(
            const PipelineLayoutDescriptor* descriptor);
        void UncachePipelineLayout(PipelineLayoutBase* obj);

        ResultOrError<RenderPipelineBase*> GetOrCreateRenderPipeline(
            const RenderPipelineDescriptor* descriptor);
        void UncacheRenderPipeline(RenderPipelineBase* obj);

        ResultOrError<SamplerBase*> GetOrCreateSampler(const SamplerDescriptor* descriptor);
        void UncacheSampler(SamplerBase* obj);

        ResultOrError<ShaderModuleBase*> GetOrCreateShaderModule(
            const ShaderModuleDescriptor* descriptor);
        void UncacheShaderModule(ShaderModuleBase* obj);

        // Dawn API
        BindGroupBase* CreateBindGroup(const BindGroupDescriptor* descriptor);
        BindGroupLayoutBase* CreateBindGroupLayout(const BindGroupLayoutDescriptor* descriptor);
        BufferBase* CreateBuffer(const BufferDescriptor* descriptor);
        DawnCreateBufferMappedResult CreateBufferMapped(const BufferDescriptor* descriptor);
        void CreateBufferMappedAsync(const BufferDescriptor* descriptor,
                                     dawn::BufferCreateMappedCallback callback,
                                     void* userdata);
        CommandEncoderBase* CreateCommandEncoder(const CommandEncoderDescriptor* descriptor);
        ComputePipelineBase* CreateComputePipeline(const ComputePipelineDescriptor* descriptor);
        PipelineLayoutBase* CreatePipelineLayout(const PipelineLayoutDescriptor* descriptor);
        QueueBase* CreateQueue();
        RenderPipelineBase* CreateRenderPipeline(const RenderPipelineDescriptor* descriptor);
        SamplerBase* CreateSampler(const SamplerDescriptor* descriptor);
        ShaderModuleBase* CreateShaderModule(const ShaderModuleDescriptor* descriptor);
        SwapChainBase* CreateSwapChain(const SwapChainDescriptor* descriptor);
        TextureBase* CreateTexture(const TextureDescriptor* descriptor);
        TextureViewBase* CreateTextureView(TextureBase* texture,
                                           const TextureViewDescriptor* descriptor);

        void Tick();

        void SetErrorCallback(dawn::DeviceErrorCallback callback, void* userdata);
        void Reference();
        void Release();

        virtual ResultOrError<std::unique_ptr<StagingBufferBase>> CreateStagingBuffer(
            size_t size) = 0;
        virtual MaybeError CopyFromStagingToBuffer(StagingBufferBase* source,
                                                   uint64_t sourceOffset,
                                                   BufferBase* destination,
                                                   uint64_t destinationOffset,
                                                   uint64_t size) = 0;

        ResultOrError<DynamicUploader*> GetDynamicUploader() const;

        std::vector<const char*> GetTogglesUsed() const;
        bool IsToggleEnabled(Toggle toggle) const;

      protected:
        void SetToggle(Toggle toggle, bool isEnabled);
        void ApplyToggleOverrides(const DeviceDescriptor* deviceDescriptor);

        std::unique_ptr<DynamicUploader> mDynamicUploader;

      private:
        virtual ResultOrError<BindGroupBase*> CreateBindGroupImpl(
            const BindGroupDescriptor* descriptor) = 0;
        virtual ResultOrError<BindGroupLayoutBase*> CreateBindGroupLayoutImpl(
            const BindGroupLayoutDescriptor* descriptor) = 0;
        virtual ResultOrError<BufferBase*> CreateBufferImpl(const BufferDescriptor* descriptor) = 0;
        virtual ResultOrError<ComputePipelineBase*> CreateComputePipelineImpl(
            const ComputePipelineDescriptor* descriptor) = 0;
        virtual ResultOrError<PipelineLayoutBase*> CreatePipelineLayoutImpl(
            const PipelineLayoutDescriptor* descriptor) = 0;
        virtual ResultOrError<QueueBase*> CreateQueueImpl() = 0;
        virtual ResultOrError<RenderPipelineBase*> CreateRenderPipelineImpl(
            const RenderPipelineDescriptor* descriptor) = 0;
        virtual ResultOrError<SamplerBase*> CreateSamplerImpl(
            const SamplerDescriptor* descriptor) = 0;
        virtual ResultOrError<ShaderModuleBase*> CreateShaderModuleImpl(
            const ShaderModuleDescriptor* descriptor) = 0;
        virtual ResultOrError<SwapChainBase*> CreateSwapChainImpl(
            const SwapChainDescriptor* descriptor) = 0;
        virtual ResultOrError<TextureBase*> CreateTextureImpl(
            const TextureDescriptor* descriptor) = 0;
        virtual ResultOrError<TextureViewBase*> CreateTextureViewImpl(
            TextureBase* texture,
            const TextureViewDescriptor* descriptor) = 0;

        MaybeError CreateBindGroupInternal(BindGroupBase** result,
                                           const BindGroupDescriptor* descriptor);
        MaybeError CreateBindGroupLayoutInternal(BindGroupLayoutBase** result,
                                                 const BindGroupLayoutDescriptor* descriptor);
        MaybeError CreateBufferInternal(BufferBase** result, const BufferDescriptor* descriptor);
        MaybeError CreateComputePipelineInternal(ComputePipelineBase** result,
                                                 const ComputePipelineDescriptor* descriptor);
        MaybeError CreatePipelineLayoutInternal(PipelineLayoutBase** result,
                                                const PipelineLayoutDescriptor* descriptor);
        MaybeError CreateQueueInternal(QueueBase** result);
        MaybeError CreateRenderPipelineInternal(RenderPipelineBase** result,
                                                const RenderPipelineDescriptor* descriptor);
        MaybeError CreateSamplerInternal(SamplerBase** result, const SamplerDescriptor* descriptor);
        MaybeError CreateShaderModuleInternal(ShaderModuleBase** result,
                                              const ShaderModuleDescriptor* descriptor);
        MaybeError CreateSwapChainInternal(SwapChainBase** result,
                                           const SwapChainDescriptor* descriptor);
        MaybeError CreateTextureInternal(TextureBase** result, const TextureDescriptor* descriptor);
        MaybeError CreateTextureViewInternal(TextureViewBase** result,
                                             TextureBase* texture,
                                             const TextureViewDescriptor* descriptor);

        void ConsumeError(ErrorData* error);
        void SetDefaultToggles();

        AdapterBase* mAdapter = nullptr;

        // The object caches aren't exposed in the header as they would require a lot of
        // additional includes.
        struct Caches;
        std::unique_ptr<Caches> mCaches;

        struct DeferredCreateBufferMappedAsync {
            dawn::BufferCreateMappedCallback callback;
            DawnBufferMapAsyncStatus status;
            DawnCreateBufferMappedResult result;
            void* userdata;
        };

        std::unique_ptr<FenceSignalTracker> mFenceSignalTracker;
        std::vector<DeferredCreateBufferMappedAsync> mDeferredCreateBufferMappedAsyncResults;

        dawn::DeviceErrorCallback mErrorCallback = nullptr;
        void* mErrorUserdata = 0;
        uint32_t mRefCount = 1;

        FormatTable mFormatTable;

        TogglesSet mTogglesSet;
    };

}  // namespace dawn_native

#endif  // DAWNNATIVE_DEVICE_H_
