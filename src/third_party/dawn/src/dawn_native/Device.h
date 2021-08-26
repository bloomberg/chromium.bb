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

#include "dawn_native/Commands.h"
#include "dawn_native/Error.h"
#include "dawn_native/Extensions.h"
#include "dawn_native/Format.h"
#include "dawn_native/Forward.h"
#include "dawn_native/ObjectBase.h"
#include "dawn_native/Toggles.h"

#include "dawn_native/DawnNative.h"
#include "dawn_native/dawn_platform.h"

#include <utility>

namespace dawn_platform {
    class WorkerTaskPool;
}  // namespace dawn_platform

namespace dawn_native {
    class AdapterBase;
    class AsyncTaskManager;
    class AttachmentState;
    class AttachmentStateBlueprint;
    class BindGroupLayoutBase;
    class CallbackTaskManager;
    class DynamicUploader;
    class ErrorScopeStack;
    class ExternalTextureBase;
    class OwnedCompilationMessages;
    class PersistentCache;
    class StagingBufferBase;
    struct CallbackTask;
    struct InternalPipelineStore;
    struct ShaderModuleParseResult;

    class DeviceBase : public RefCounted {
      public:
        DeviceBase(AdapterBase* adapter, const DeviceDescriptor* descriptor);
        virtual ~DeviceBase();

        void HandleError(InternalErrorType type, const char* message);

        bool ConsumedError(MaybeError maybeError) {
            if (DAWN_UNLIKELY(maybeError.IsError())) {
                ConsumeError(maybeError.AcquireError());
                return true;
            }
            return false;
        }

        template <typename T>
        bool ConsumedError(ResultOrError<T> resultOrError, T* result) {
            if (DAWN_UNLIKELY(resultOrError.IsError())) {
                ConsumeError(resultOrError.AcquireError());
                return true;
            }
            *result = resultOrError.AcquireSuccess();
            return false;
        }

        MaybeError ValidateObject(const ObjectBase* object) const;

        AdapterBase* GetAdapter() const;
        dawn_platform::Platform* GetPlatform() const;

        // Returns the Format corresponding to the wgpu::TextureFormat or an error if the format
        // isn't a valid wgpu::TextureFormat or isn't supported by this device.
        // The pointer returned has the same lifetime as the device.
        ResultOrError<const Format*> GetInternalFormat(wgpu::TextureFormat format) const;

        // Returns the Format corresponding to the wgpu::TextureFormat and assumes the format is
        // valid and supported.
        // The reference returned has the same lifetime as the device.
        const Format& GetValidInternalFormat(wgpu::TextureFormat format) const;

        virtual ResultOrError<Ref<CommandBufferBase>> CreateCommandBuffer(
            CommandEncoder* encoder,
            const CommandBufferDescriptor* descriptor) = 0;

        ExecutionSerial GetCompletedCommandSerial() const;
        ExecutionSerial GetLastSubmittedCommandSerial() const;
        ExecutionSerial GetFutureSerial() const;
        ExecutionSerial GetPendingCommandSerial() const;

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
        ResultOrError<Ref<BindGroupLayoutBase>> GetOrCreateBindGroupLayout(
            const BindGroupLayoutDescriptor* descriptor);
        void UncacheBindGroupLayout(BindGroupLayoutBase* obj);

        BindGroupLayoutBase* GetEmptyBindGroupLayout();

        void UncacheComputePipeline(ComputePipelineBase* obj);

        ResultOrError<Ref<PipelineLayoutBase>> GetOrCreatePipelineLayout(
            const PipelineLayoutDescriptor* descriptor);
        void UncachePipelineLayout(PipelineLayoutBase* obj);

        ResultOrError<Ref<RenderPipelineBase>> GetOrCreateRenderPipeline(
            const RenderPipelineDescriptor* descriptor);
        void UncacheRenderPipeline(RenderPipelineBase* obj);

        ResultOrError<Ref<SamplerBase>> GetOrCreateSampler(const SamplerDescriptor* descriptor);
        void UncacheSampler(SamplerBase* obj);

        ResultOrError<Ref<ShaderModuleBase>> GetOrCreateShaderModule(
            const ShaderModuleDescriptor* descriptor,
            ShaderModuleParseResult* parseResult,
            OwnedCompilationMessages* compilationMessages);
        void UncacheShaderModule(ShaderModuleBase* obj);

        Ref<AttachmentState> GetOrCreateAttachmentState(AttachmentStateBlueprint* blueprint);
        Ref<AttachmentState> GetOrCreateAttachmentState(
            const RenderBundleEncoderDescriptor* descriptor);
        Ref<AttachmentState> GetOrCreateAttachmentState(const RenderPipelineDescriptor* descriptor);
        Ref<AttachmentState> GetOrCreateAttachmentState(const RenderPassDescriptor* descriptor);
        void UncacheAttachmentState(AttachmentState* obj);

        // Object creation methods that be used in a reentrant manner.
        ResultOrError<Ref<BindGroupBase>> CreateBindGroup(const BindGroupDescriptor* descriptor);
        ResultOrError<Ref<BindGroupLayoutBase>> CreateBindGroupLayout(
            const BindGroupLayoutDescriptor* descriptor,
            bool allowInternalBinding = false);
        ResultOrError<Ref<BufferBase>> CreateBuffer(const BufferDescriptor* descriptor);
        ResultOrError<Ref<ComputePipelineBase>> CreateComputePipeline(
            const ComputePipelineDescriptor* descriptor);
        MaybeError CreateComputePipelineAsync(
            const ComputePipelineDescriptor* descriptor,
            WGPUCreateComputePipelineAsyncCallback callback,
            void* userdata);
        ResultOrError<Ref<ExternalTextureBase>> CreateExternalTexture(
            const ExternalTextureDescriptor* descriptor);
        ResultOrError<Ref<PipelineLayoutBase>> CreatePipelineLayout(
            const PipelineLayoutDescriptor* descriptor);
        ResultOrError<Ref<QuerySetBase>> CreateQuerySet(const QuerySetDescriptor* descriptor);
        ResultOrError<Ref<RenderBundleEncoder>> CreateRenderBundleEncoder(
            const RenderBundleEncoderDescriptor* descriptor);
        ResultOrError<Ref<RenderPipelineBase>> CreateRenderPipeline(
            const RenderPipelineDescriptor* descriptor);
        ResultOrError<Ref<SamplerBase>> CreateSampler(const SamplerDescriptor* descriptor);
        ResultOrError<Ref<ShaderModuleBase>> CreateShaderModule(
            const ShaderModuleDescriptor* descriptor,
            OwnedCompilationMessages* compilationMessages = nullptr);
        ResultOrError<Ref<SwapChainBase>> CreateSwapChain(Surface* surface,
                                                          const SwapChainDescriptor* descriptor);
        ResultOrError<Ref<TextureBase>> CreateTexture(const TextureDescriptor* descriptor);
        ResultOrError<Ref<TextureViewBase>> CreateTextureView(
            TextureBase* texture,
            const TextureViewDescriptor* descriptor);

        // Implementation of API object creation methods. DO NOT use them in a reentrant manner.
        BindGroupBase* APICreateBindGroup(const BindGroupDescriptor* descriptor);
        BindGroupLayoutBase* APICreateBindGroupLayout(const BindGroupLayoutDescriptor* descriptor);
        BufferBase* APICreateBuffer(const BufferDescriptor* descriptor);
        CommandEncoder* APICreateCommandEncoder(const CommandEncoderDescriptor* descriptor);
        ComputePipelineBase* APICreateComputePipeline(const ComputePipelineDescriptor* descriptor);
        PipelineLayoutBase* APICreatePipelineLayout(const PipelineLayoutDescriptor* descriptor);
        QuerySetBase* APICreateQuerySet(const QuerySetDescriptor* descriptor);
        void APICreateComputePipelineAsync(const ComputePipelineDescriptor* descriptor,
                                           WGPUCreateComputePipelineAsyncCallback callback,
                                           void* userdata);
        void APICreateRenderPipelineAsync(const RenderPipelineDescriptor* descriptor,
                                          WGPUCreateRenderPipelineAsyncCallback callback,
                                          void* userdata);
        RenderBundleEncoder* APICreateRenderBundleEncoder(
            const RenderBundleEncoderDescriptor* descriptor);
        RenderPipelineBase* APICreateRenderPipeline(const RenderPipelineDescriptor* descriptor);
        ExternalTextureBase* APICreateExternalTexture(const ExternalTextureDescriptor* descriptor);
        SamplerBase* APICreateSampler(const SamplerDescriptor* descriptor);
        ShaderModuleBase* APICreateShaderModule(const ShaderModuleDescriptor* descriptor);
        SwapChainBase* APICreateSwapChain(Surface* surface, const SwapChainDescriptor* descriptor);
        TextureBase* APICreateTexture(const TextureDescriptor* descriptor);

        InternalPipelineStore* GetInternalPipelineStore();

        // For Dawn Wire
        BufferBase* APICreateErrorBuffer();

        QueueBase* APIGetQueue();

        void APIInjectError(wgpu::ErrorType type, const char* message);
        bool APITick();

        void APISetDeviceLostCallback(wgpu::DeviceLostCallback callback, void* userdata);
        void APISetUncapturedErrorCallback(wgpu::ErrorCallback callback, void* userdata);
        void APISetLoggingCallback(wgpu::LoggingCallback callback, void* userdata);
        void APIPushErrorScope(wgpu::ErrorFilter filter);
        bool APIPopErrorScope(wgpu::ErrorCallback callback, void* userdata);

        MaybeError ValidateIsAlive() const;

        PersistentCache* GetPersistentCache();

        virtual ResultOrError<std::unique_ptr<StagingBufferBase>> CreateStagingBuffer(
            size_t size) = 0;
        virtual MaybeError CopyFromStagingToBuffer(StagingBufferBase* source,
                                                   uint64_t sourceOffset,
                                                   BufferBase* destination,
                                                   uint64_t destinationOffset,
                                                   uint64_t size) = 0;
        virtual MaybeError CopyFromStagingToTexture(const StagingBufferBase* source,
                                                    const TextureDataLayout& src,
                                                    TextureCopy* dst,
                                                    const Extent3D& copySizePixels) = 0;

        DynamicUploader* GetDynamicUploader() const;

        // The device state which is a combination of creation state and loss state.
        //
        //   - BeingCreated: the device didn't finish creation yet and the frontend cannot be used
        //     (both for the application calling WebGPU, or re-entrant calls). No work exists on
        //     the GPU timeline.
        //   - Alive: the device is usable and might have work happening on the GPU timeline.
        //   - BeingDisconnected: the device is no longer usable because we are waiting for all
        //     work on the GPU timeline to finish. (this is to make validation prevent the
        //     application from adding more work during the transition from Available to
        //     Disconnected)
        //   - Disconnected: there is no longer work happening on the GPU timeline and the CPU data
        //     structures can be safely destroyed without additional synchronization.
        enum class State {
            BeingCreated,
            Alive,
            BeingDisconnected,
            Disconnected,
        };
        State GetState() const;
        bool IsLost() const;

        std::vector<const char*> GetEnabledExtensions() const;
        std::vector<const char*> GetTogglesUsed() const;
        bool IsExtensionEnabled(Extension extension) const;
        bool IsToggleEnabled(Toggle toggle) const;
        bool IsValidationEnabled() const;
        bool IsRobustnessEnabled() const;
        size_t GetLazyClearCountForTesting();
        void IncrementLazyClearCountForTesting();
        size_t GetDeprecationWarningCountForTesting();
        void EmitDeprecationWarning(const char* warning);
        void EmitLog(const char* message);
        void EmitLog(WGPULoggingType loggingType, const char* message);
        void APILoseForTesting();
        QueueBase* GetQueue() const;

        // AddFutureSerial is used to update the mFutureSerial with the max serial needed to be
        // ticked in order to clean up all pending callback work or to execute asynchronous resource
        // writes. It should be given the serial that a callback is tracked with, so that once that
        // serial is completed, it can be resolved and cleaned up. This is so that when there is no
        // gpu work (the last submitted serial has not moved beyond the completed serial), Tick can
        // still check if we have pending work to take care of, rather than hanging and never
        // reaching the serial the work will be executed on.
        void AddFutureSerial(ExecutionSerial serial);
        // Check for passed fences and set the new completed serial
        MaybeError CheckPassedSerials();

        MaybeError Tick();

        virtual uint32_t GetOptimalBytesPerRowAlignment() const = 0;
        virtual uint64_t GetOptimalBufferToTextureCopyOffsetAlignment() const = 0;

        virtual float GetTimestampPeriodInNS() const = 0;

        AsyncTaskManager* GetAsyncTaskManager() const;
        CallbackTaskManager* GetCallbackTaskManager() const;
        dawn_platform::WorkerTaskPool* GetWorkerTaskPool() const;

        void AddComputePipelineAsyncCallbackTask(Ref<ComputePipelineBase> pipeline,
                                                 std::string errorMessage,
                                                 WGPUCreateComputePipelineAsyncCallback callback,
                                                 void* userdata,
                                                 size_t blueprintHash);

      protected:
        void SetToggle(Toggle toggle, bool isEnabled);
        void ForceSetToggle(Toggle toggle, bool isEnabled);

        MaybeError Initialize(QueueBase* defaultQueue);
        void ShutDownBase();

        // Incrememt mLastSubmittedSerial when we submit the next serial
        void IncrementLastSubmittedCommandSerial();

      private:
        virtual ResultOrError<Ref<BindGroupBase>> CreateBindGroupImpl(
            const BindGroupDescriptor* descriptor) = 0;
        virtual ResultOrError<Ref<BindGroupLayoutBase>> CreateBindGroupLayoutImpl(
            const BindGroupLayoutDescriptor* descriptor) = 0;
        virtual ResultOrError<Ref<BufferBase>> CreateBufferImpl(
            const BufferDescriptor* descriptor) = 0;
        virtual ResultOrError<Ref<ComputePipelineBase>> CreateComputePipelineImpl(
            const ComputePipelineDescriptor* descriptor) = 0;
        virtual ResultOrError<Ref<PipelineLayoutBase>> CreatePipelineLayoutImpl(
            const PipelineLayoutDescriptor* descriptor) = 0;
        virtual ResultOrError<Ref<QuerySetBase>> CreateQuerySetImpl(
            const QuerySetDescriptor* descriptor) = 0;
        virtual ResultOrError<Ref<RenderPipelineBase>> CreateRenderPipelineImpl(
            const RenderPipelineDescriptor* descriptor) = 0;
        virtual ResultOrError<Ref<SamplerBase>> CreateSamplerImpl(
            const SamplerDescriptor* descriptor) = 0;
        virtual ResultOrError<Ref<ShaderModuleBase>> CreateShaderModuleImpl(
            const ShaderModuleDescriptor* descriptor,
            ShaderModuleParseResult* parseResult) = 0;
        virtual ResultOrError<Ref<SwapChainBase>> CreateSwapChainImpl(
            const SwapChainDescriptor* descriptor) = 0;
        // Note that previousSwapChain may be nullptr, or come from a different backend.
        virtual ResultOrError<Ref<NewSwapChainBase>> CreateSwapChainImpl(
            Surface* surface,
            NewSwapChainBase* previousSwapChain,
            const SwapChainDescriptor* descriptor) = 0;
        virtual ResultOrError<Ref<TextureBase>> CreateTextureImpl(
            const TextureDescriptor* descriptor) = 0;
        virtual ResultOrError<Ref<TextureViewBase>> CreateTextureViewImpl(
            TextureBase* texture,
            const TextureViewDescriptor* descriptor) = 0;

        virtual MaybeError TickImpl() = 0;
        void FlushCallbackTaskQueue();

        ResultOrError<Ref<BindGroupLayoutBase>> CreateEmptyBindGroupLayout();

        ResultOrError<Ref<PipelineLayoutBase>> ValidateAndGetComputePipelineDescriptorWithDefaults(
            const ComputePipelineDescriptor& descriptor,
            ComputePipelineDescriptor* outDescriptor);
        std::pair<Ref<ComputePipelineBase>, size_t> GetCachedComputePipeline(
            const ComputePipelineDescriptor* descriptor);
        Ref<ComputePipelineBase> AddOrGetCachedPipeline(Ref<ComputePipelineBase> computePipeline,
                                                        size_t blueprintHash);
        virtual void CreateComputePipelineAsyncImpl(const ComputePipelineDescriptor* descriptor,
                                                    size_t blueprintHash,
                                                    WGPUCreateComputePipelineAsyncCallback callback,
                                                    void* userdata);

        void ApplyToggleOverrides(const DeviceDescriptor* deviceDescriptor);
        void ApplyExtensions(const DeviceDescriptor* deviceDescriptor);

        void SetDefaultToggles();

        void ConsumeError(std::unique_ptr<ErrorData> error);

        // Each backend should implement to check their passed fences if there are any and return a
        // completed serial. Return 0 should indicate no fences to check.
        virtual ResultOrError<ExecutionSerial> CheckAndUpdateCompletedSerials() = 0;
        // During shut down of device, some operations might have been started since the last submit
        // and waiting on a serial that doesn't have a corresponding fence enqueued. Fake serials to
        // make all commands look completed.
        void AssumeCommandsComplete();
        bool IsDeviceIdle();

        // mCompletedSerial tracks the last completed command serial that the fence has returned.
        // mLastSubmittedSerial tracks the last submitted command serial.
        // During device removal, the serials could be artificially incremented
        // to make it appear as if commands have been compeleted. They can also be artificially
        // incremented when no work is being done in the GPU so CPU operations don't have to wait on
        // stale serials.
        // mFutureSerial tracks the largest serial we need to tick to for asynchronous commands or
        // callbacks to fire
        ExecutionSerial mCompletedSerial = ExecutionSerial(0);
        ExecutionSerial mLastSubmittedSerial = ExecutionSerial(0);
        ExecutionSerial mFutureSerial = ExecutionSerial(0);

        // ShutDownImpl is used to clean up and release resources used by device, does not wait for
        // GPU or check errors.
        virtual void ShutDownImpl() = 0;

        // WaitForIdleForDestruction waits for GPU to finish, checks errors and gets ready for
        // destruction. This is only used when properly destructing the device. For a real
        // device loss, this function doesn't need to be called since the driver already closed all
        // resources.
        virtual MaybeError WaitForIdleForDestruction() = 0;

        wgpu::ErrorCallback mUncapturedErrorCallback = nullptr;
        void* mUncapturedErrorUserdata = nullptr;

        wgpu::LoggingCallback mLoggingCallback = nullptr;
        void* mLoggingUserdata = nullptr;

        wgpu::DeviceLostCallback mDeviceLostCallback = nullptr;
        void* mDeviceLostUserdata = nullptr;

        std::unique_ptr<ErrorScopeStack> mErrorScopeStack;

        // The Device keeps a ref to the Instance so that any live Device keeps the Instance alive.
        // The Instance shouldn't need to ref child objects so this shouldn't introduce ref cycles.
        // The Device keeps a simple pointer to the Adapter because the Adapter is owned by the
        // Instance.
        Ref<InstanceBase> mInstance;
        AdapterBase* mAdapter = nullptr;

        // The object caches aren't exposed in the header as they would require a lot of
        // additional includes.
        struct Caches;
        std::unique_ptr<Caches> mCaches;

        Ref<BindGroupLayoutBase> mEmptyBindGroupLayout;

        std::unique_ptr<DynamicUploader> mDynamicUploader;
        std::unique_ptr<AsyncTaskManager> mAsyncTaskManager;
        Ref<QueueBase> mQueue;

        struct DeprecationWarnings;
        std::unique_ptr<DeprecationWarnings> mDeprecationWarnings;

        State mState = State::BeingCreated;

        FormatTable mFormatTable;

        TogglesSet mEnabledToggles;
        TogglesSet mOverridenToggles;
        size_t mLazyClearCountForTesting = 0;

        ExtensionsSet mEnabledExtensions;

        std::unique_ptr<InternalPipelineStore> mInternalPipelineStore;

        std::unique_ptr<PersistentCache> mPersistentCache;

        std::unique_ptr<CallbackTaskManager> mCallbackTaskManager;
        std::unique_ptr<dawn_platform::WorkerTaskPool> mWorkerTaskPool;
    };

}  // namespace dawn_native

#endif  // DAWNNATIVE_DEVICE_H_
