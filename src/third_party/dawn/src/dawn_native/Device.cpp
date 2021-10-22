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

#include "dawn_native/Device.h"

#include "common/Log.h"
#include "dawn_native/Adapter.h"
#include "dawn_native/AsyncTask.h"
#include "dawn_native/AttachmentState.h"
#include "dawn_native/BindGroup.h"
#include "dawn_native/BindGroupLayout.h"
#include "dawn_native/Buffer.h"
#include "dawn_native/CommandBuffer.h"
#include "dawn_native/CommandEncoder.h"
#include "dawn_native/CompilationMessages.h"
#include "dawn_native/ComputePipeline.h"
#include "dawn_native/CreatePipelineAsyncTask.h"
#include "dawn_native/DynamicUploader.h"
#include "dawn_native/ErrorData.h"
#include "dawn_native/ErrorInjector.h"
#include "dawn_native/ErrorScope.h"
#include "dawn_native/ExternalTexture.h"
#include "dawn_native/Instance.h"
#include "dawn_native/InternalPipelineStore.h"
#include "dawn_native/ObjectType_autogen.h"
#include "dawn_native/PersistentCache.h"
#include "dawn_native/PipelineLayout.h"
#include "dawn_native/QuerySet.h"
#include "dawn_native/Queue.h"
#include "dawn_native/RenderBundleEncoder.h"
#include "dawn_native/RenderPipeline.h"
#include "dawn_native/Sampler.h"
#include "dawn_native/ShaderModule.h"
#include "dawn_native/Surface.h"
#include "dawn_native/SwapChain.h"
#include "dawn_native/Texture.h"
#include "dawn_native/ValidationUtils_autogen.h"
#include "dawn_platform/DawnPlatform.h"

#include <mutex>
#include <unordered_set>

namespace dawn_native {

    // DeviceBase sub-structures

    // The caches are unordered_sets of pointers with special hash and compare functions
    // to compare the value of the objects, instead of the pointers.
    template <typename Object>
    using ContentLessObjectCache =
        std::unordered_set<Object*, typename Object::HashFunc, typename Object::EqualityFunc>;

    struct DeviceBase::Caches {
        ~Caches() {
            ASSERT(attachmentStates.empty());
            ASSERT(bindGroupLayouts.empty());
            ASSERT(computePipelines.empty());
            ASSERT(pipelineLayouts.empty());
            ASSERT(renderPipelines.empty());
            ASSERT(samplers.empty());
            ASSERT(shaderModules.empty());
        }

        ContentLessObjectCache<AttachmentStateBlueprint> attachmentStates;
        ContentLessObjectCache<BindGroupLayoutBase> bindGroupLayouts;
        ContentLessObjectCache<ComputePipelineBase> computePipelines;
        ContentLessObjectCache<PipelineLayoutBase> pipelineLayouts;
        ContentLessObjectCache<RenderPipelineBase> renderPipelines;
        ContentLessObjectCache<SamplerBase> samplers;
        ContentLessObjectCache<ShaderModuleBase> shaderModules;
    };

    struct DeviceBase::DeprecationWarnings {
        std::unordered_set<std::string> emitted;
        size_t count = 0;
    };

    namespace {
        struct LoggingCallbackTask : CallbackTask {
          public:
            LoggingCallbackTask() = delete;
            LoggingCallbackTask(wgpu::LoggingCallback loggingCallback,
                                WGPULoggingType loggingType,
                                const char* message,
                                void* userdata)
                : mCallback(loggingCallback),
                  mLoggingType(loggingType),
                  mMessage(message),
                  mUserdata(userdata) {
                // Since the Finish() will be called in uncertain future in which time the message
                // may already disposed, we must keep a local copy in the CallbackTask.
            }

            void Finish() override {
                mCallback(mLoggingType, mMessage.c_str(), mUserdata);
            }

            void HandleShutDown() override {
                // Do the logging anyway
                mCallback(mLoggingType, mMessage.c_str(), mUserdata);
            }

            void HandleDeviceLoss() override {
                mCallback(mLoggingType, mMessage.c_str(), mUserdata);
            }

          private:
            // As all deferred callback tasks will be triggered before modifying the registered
            // callback or shutting down, we are ensured that callback function and userdata pointer
            // stored in tasks is valid when triggered.
            wgpu::LoggingCallback mCallback;
            WGPULoggingType mLoggingType;
            std::string mMessage;
            void* mUserdata;
        };

        ResultOrError<Ref<PipelineLayoutBase>>
        ValidateLayoutAndGetComputePipelineDescriptorWithDefaults(
            DeviceBase* device,
            const ComputePipelineDescriptor& descriptor,
            ComputePipelineDescriptor* outDescriptor) {
            Ref<PipelineLayoutBase> layoutRef;
            *outDescriptor = descriptor;

            if (outDescriptor->layout == nullptr) {
                DAWN_TRY_ASSIGN(layoutRef, PipelineLayoutBase::CreateDefault(
                                               device, {{
                                                           SingleShaderStage::Compute,
                                                           outDescriptor->compute.module,
                                                           outDescriptor->compute.entryPoint,
                                                           outDescriptor->compute.constantCount,
                                                           outDescriptor->compute.constants,
                                                       }}));
                outDescriptor->layout = layoutRef.Get();
            }

            return layoutRef;
        }

        ResultOrError<Ref<PipelineLayoutBase>>
        ValidateLayoutAndGetRenderPipelineDescriptorWithDefaults(
            DeviceBase* device,
            const RenderPipelineDescriptor& descriptor,
            RenderPipelineDescriptor* outDescriptor) {
            Ref<PipelineLayoutBase> layoutRef;
            *outDescriptor = descriptor;

            if (descriptor.layout == nullptr) {
                // Ref will keep the pipeline layout alive until the end of the function where
                // the pipeline will take another reference.
                DAWN_TRY_ASSIGN(layoutRef,
                                PipelineLayoutBase::CreateDefault(
                                    device, GetRenderStagesAndSetDummyShader(device, &descriptor)));
                outDescriptor->layout = layoutRef.Get();
            }

            return layoutRef;
        }

    }  // anonymous namespace

    // DeviceBase

    DeviceBase::DeviceBase(AdapterBase* adapter, const DeviceDescriptor* descriptor)
        : mInstance(adapter->GetInstance()), mAdapter(adapter), mNextPipelineCompatibilityToken(1) {
        if (descriptor != nullptr) {
            ApplyToggleOverrides(descriptor);
            ApplyFeatures(descriptor);
        }

        if (descriptor != nullptr && descriptor->requiredLimits != nullptr) {
            mLimits.v1 = ReifyDefaultLimits(
                reinterpret_cast<const RequiredLimits*>(descriptor->requiredLimits)->limits);
        } else {
            GetDefaultLimits(&mLimits.v1);
        }

        mFormatTable = BuildFormatTable(this);
        SetDefaultToggles();
        if ((adapter->GetBackendType() == wgpu::BackendType::Metal ||
             adapter->GetBackendType() == wgpu::BackendType::Vulkan ||
             adapter->GetBackendType() == wgpu::BackendType::D3D12) &&
            !IsToggleEnabled(Toggle::UseTintGenerator)) {
            EmitLog(
                WGPULoggingType_Warning,
                "Non-tint generator is not available on this backend; toggle disable ignored.\n");
            ForceSetToggle(Toggle::UseTintGenerator, true);
        }
    }

    DeviceBase::~DeviceBase() = default;

    MaybeError DeviceBase::Initialize(QueueBase* defaultQueue) {
        mQueue = AcquireRef(defaultQueue);

#if defined(DAWN_ENABLE_ASSERTS)
        mUncapturedErrorCallback = [](WGPUErrorType, char const*, void*) {
            static bool calledOnce = false;
            if (!calledOnce) {
                calledOnce = true;
                dawn::WarningLog() << "No Dawn device uncaptured error callback was set. This is "
                                      "probably not intended. If you really want to ignore errors "
                                      "and suppress this message, set the callback to null.";
            }
        };

        mDeviceLostCallback = [](WGPUDeviceLostReason, char const*, void*) {
            static bool calledOnce = false;
            if (!calledOnce) {
                calledOnce = true;
                dawn::WarningLog() << "No Dawn device lost callback was set. This is probably not "
                                      "intended. If you really want to ignore device lost "
                                      "and suppress this message, set the callback to null.";
            }
        };
#endif  // DAWN_ENABLE_ASSERTS

        mCaches = std::make_unique<DeviceBase::Caches>();
        mErrorScopeStack = std::make_unique<ErrorScopeStack>();
        mDynamicUploader = std::make_unique<DynamicUploader>(this);
        mCallbackTaskManager = std::make_unique<CallbackTaskManager>();
        mDeprecationWarnings = std::make_unique<DeprecationWarnings>();
        mInternalPipelineStore = std::make_unique<InternalPipelineStore>(this);
        mPersistentCache = std::make_unique<PersistentCache>(this);

        ASSERT(GetPlatform() != nullptr);
        mWorkerTaskPool = GetPlatform()->CreateWorkerTaskPool();
        mAsyncTaskManager = std::make_unique<AsyncTaskManager>(mWorkerTaskPool.get());

        // Starting from now the backend can start doing reentrant calls so the device is marked as
        // alive.
        mState = State::Alive;

        DAWN_TRY_ASSIGN(mEmptyBindGroupLayout, CreateEmptyBindGroupLayout());

        // If dummy fragment shader module is needed, initialize it
        if (IsToggleEnabled(Toggle::UseDummyFragmentInVertexOnlyPipeline)) {
            // The empty fragment shader, used as a work around for vertex-only render pipeline
            constexpr char kEmptyFragmentShader[] = R"(
                [[stage(fragment)]] fn fs_empty_main() {}
            )";
            ShaderModuleDescriptor descriptor;
            ShaderModuleWGSLDescriptor wgslDesc;
            wgslDesc.source = kEmptyFragmentShader;
            descriptor.nextInChain = reinterpret_cast<ChainedStruct*>(&wgslDesc);

            DAWN_TRY_ASSIGN(mInternalPipelineStore->dummyFragmentShader,
                            CreateShaderModule(&descriptor));
        }

        return {};
    }

    void DeviceBase::ShutDownBase() {
        // Skip handling device facilities if they haven't even been created (or failed doing so)
        if (mState != State::BeingCreated) {
            // Call all the callbacks immediately as the device is about to shut down.
            // TODO(crbug.com/dawn/826): Cancel the tasks that are in flight if possible.
            mAsyncTaskManager->WaitAllPendingTasks();
            auto callbackTasks = mCallbackTaskManager->AcquireCallbackTasks();
            for (std::unique_ptr<CallbackTask>& callbackTask : callbackTasks) {
                callbackTask->HandleShutDown();
            }
        }

        // Disconnect the device, depending on which state we are currently in.
        switch (mState) {
            case State::BeingCreated:
                // The GPU timeline was never started so we don't have to wait.
                break;

            case State::Alive:
                // Alive is the only state which can have GPU work happening. Wait for all of it to
                // complete before proceeding with destruction.
                // Ignore errors so that we can continue with destruction
                IgnoreErrors(WaitForIdleForDestruction());
                AssumeCommandsComplete();
                break;

            case State::BeingDisconnected:
                // Getting disconnected is a transient state happening in a single API call so there
                // is always an external reference keeping the Device alive, which means the
                // destructor cannot run while BeingDisconnected.
                UNREACHABLE();
                break;

            case State::Disconnected:
                break;
        }
        ASSERT(mCompletedSerial == mLastSubmittedSerial);
        ASSERT(mFutureSerial <= mCompletedSerial);

        if (mState != State::BeingCreated) {
            // The GPU timeline is finished.
            // Tick the queue-related tasks since they should be complete. This must be done before
            // ShutDownImpl() it may relinquish resources that will be freed by backends in the
            // ShutDownImpl() call.
            mQueue->Tick(GetCompletedCommandSerial());
            // Call TickImpl once last time to clean up resources
            // Ignore errors so that we can continue with destruction
            IgnoreErrors(TickImpl());
        }

        // At this point GPU operations are always finished, so we are in the disconnected state.
        mState = State::Disconnected;

        mDynamicUploader = nullptr;
        mCallbackTaskManager = nullptr;
        mAsyncTaskManager = nullptr;
        mPersistentCache = nullptr;

        mEmptyBindGroupLayout = nullptr;

        mInternalPipelineStore = nullptr;

        AssumeCommandsComplete();
        // Tell the backend that it can free all the objects now that the GPU timeline is empty.
        ShutDownImpl();

        mCaches = nullptr;
    }

    void DeviceBase::HandleError(InternalErrorType type, const char* message) {
        if (type == InternalErrorType::DeviceLost) {
            mState = State::Disconnected;

            // If the ErrorInjector is enabled, then the device loss might be fake and the device
            // still be executing commands. Force a wait for idle in this case, with State being
            // Disconnected so we can detect this case in WaitForIdleForDestruction.
            if (ErrorInjectorEnabled()) {
                IgnoreErrors(WaitForIdleForDestruction());
            }

            // A real device lost happened. Set the state to disconnected as the device cannot be
            // used. Also tags all commands as completed since the device stopped running.
            AssumeCommandsComplete();
        } else if (type == InternalErrorType::Internal) {
            // If we receive an internal error, assume the backend can't recover and proceed with
            // device destruction. We first wait for all previous commands to be completed so that
            // backend objects can be freed immediately, before handling the loss.

            // Move away from the Alive state so that the application cannot use this device
            // anymore.
            // TODO(crbug.com/dawn/831): Do we need atomics for this to become visible to other
            // threads in a multithreaded scenario?
            mState = State::BeingDisconnected;

            // Ignore errors so that we can continue with destruction
            // Assume all commands are complete after WaitForIdleForDestruction (because they were)
            IgnoreErrors(WaitForIdleForDestruction());
            IgnoreErrors(TickImpl());
            AssumeCommandsComplete();
            ASSERT(mFutureSerial <= mCompletedSerial);
            mState = State::Disconnected;

            // Now everything is as if the device was lost.
            type = InternalErrorType::DeviceLost;
        }

        if (type == InternalErrorType::DeviceLost) {
            // The device was lost, call the application callback.
            if (mDeviceLostCallback != nullptr) {
                // TODO(crbug.com/dawn/628): Make sure the "Destroyed" reason is passed if
                // the device was destroyed.
                mDeviceLostCallback(WGPUDeviceLostReason_Undefined, message, mDeviceLostUserdata);
                mDeviceLostCallback = nullptr;
            }

            mQueue->HandleDeviceLoss();

            // TODO(crbug.com/dawn/826): Cancel the tasks that are in flight if possible.
            mAsyncTaskManager->WaitAllPendingTasks();
            auto callbackTasks = mCallbackTaskManager->AcquireCallbackTasks();
            for (std::unique_ptr<CallbackTask>& callbackTask : callbackTasks) {
                callbackTask->HandleDeviceLoss();
            }

            // Still forward device loss errors to the error scopes so they all reject.
            mErrorScopeStack->HandleError(ToWGPUErrorType(type), message);
        } else {
            // Pass the error to the error scope stack and call the uncaptured error callback
            // if it isn't handled. DeviceLost is not handled here because it should be
            // handled by the lost callback.
            bool captured = mErrorScopeStack->HandleError(ToWGPUErrorType(type), message);
            if (!captured && mUncapturedErrorCallback != nullptr) {
                mUncapturedErrorCallback(static_cast<WGPUErrorType>(ToWGPUErrorType(type)), message,
                                         mUncapturedErrorUserdata);
            }
        }
    }

    void DeviceBase::ConsumeError(std::unique_ptr<ErrorData> error) {
        ASSERT(error != nullptr);
        HandleError(error->GetType(), error->GetFormattedMessage().c_str());
    }

    void DeviceBase::APISetLoggingCallback(wgpu::LoggingCallback callback, void* userdata) {
        // The registered callback function and userdata pointer are stored and used by deferred
        // callback tasks, and after setting a different callback (especially in the case of
        // resetting) the resources pointed by such pointer may be freed. Flush all deferred
        // callback tasks to guarantee we are never going to use the previous callback after
        // this call.
        FlushCallbackTaskQueue();
        mLoggingCallback = callback;
        mLoggingUserdata = userdata;
    }

    void DeviceBase::APISetUncapturedErrorCallback(wgpu::ErrorCallback callback, void* userdata) {
        // The registered callback function and userdata pointer are stored and used by deferred
        // callback tasks, and after setting a different callback (especially in the case of
        // resetting) the resources pointed by such pointer may be freed. Flush all deferred
        // callback tasks to guarantee we are never going to use the previous callback after
        // this call.
        FlushCallbackTaskQueue();
        mUncapturedErrorCallback = callback;
        mUncapturedErrorUserdata = userdata;
    }

    void DeviceBase::APISetDeviceLostCallback(wgpu::DeviceLostCallback callback, void* userdata) {
        // The registered callback function and userdata pointer are stored and used by deferred
        // callback tasks, and after setting a different callback (especially in the case of
        // resetting) the resources pointed by such pointer may be freed. Flush all deferred
        // callback tasks to guarantee we are never going to use the previous callback after
        // this call.
        FlushCallbackTaskQueue();
        mDeviceLostCallback = callback;
        mDeviceLostUserdata = userdata;
    }

    void DeviceBase::APIPushErrorScope(wgpu::ErrorFilter filter) {
        if (ConsumedError(ValidateErrorFilter(filter))) {
            return;
        }
        mErrorScopeStack->Push(filter);
    }

    bool DeviceBase::APIPopErrorScope(wgpu::ErrorCallback callback, void* userdata) {
        if (mErrorScopeStack->Empty()) {
            return false;
        }
        ErrorScope scope = mErrorScopeStack->Pop();
        if (callback != nullptr) {
            callback(static_cast<WGPUErrorType>(scope.GetErrorType()), scope.GetErrorMessage(),
                     userdata);
        }

        return true;
    }

    PersistentCache* DeviceBase::GetPersistentCache() {
        ASSERT(mPersistentCache.get() != nullptr);
        return mPersistentCache.get();
    }

    MaybeError DeviceBase::ValidateObject(const ApiObjectBase* object) const {
        ASSERT(object != nullptr);
        DAWN_INVALID_IF(object->GetDevice() != this,
                        "%s is associated with %s, and cannot be used with %s.", object,
                        object->GetDevice(), this);

        // TODO(dawn:563): Preserve labels for error objects.
        DAWN_INVALID_IF(object->IsError(), "%s is an error.", object);

        return {};
    }

    MaybeError DeviceBase::ValidateIsAlive() const {
        if (DAWN_LIKELY(mState == State::Alive)) {
            return {};
        }
        return DAWN_VALIDATION_ERROR("Device is lost");
    }

    void DeviceBase::APILoseForTesting() {
        if (mState != State::Alive) {
            return;
        }

        HandleError(InternalErrorType::Internal, "Device lost for testing");
    }

    DeviceBase::State DeviceBase::GetState() const {
        return mState;
    }

    bool DeviceBase::IsLost() const {
        ASSERT(mState != State::BeingCreated);
        return mState != State::Alive;
    }

    std::mutex* DeviceBase::GetObjectListMutex(ObjectType type) {
        return &mObjectLists[type].mutex;
    }

    AdapterBase* DeviceBase::GetAdapter() const {
        return mAdapter;
    }

    dawn_platform::Platform* DeviceBase::GetPlatform() const {
        return GetAdapter()->GetInstance()->GetPlatform();
    }

    ExecutionSerial DeviceBase::GetCompletedCommandSerial() const {
        return mCompletedSerial;
    }

    ExecutionSerial DeviceBase::GetLastSubmittedCommandSerial() const {
        return mLastSubmittedSerial;
    }

    ExecutionSerial DeviceBase::GetFutureSerial() const {
        return mFutureSerial;
    }

    InternalPipelineStore* DeviceBase::GetInternalPipelineStore() {
        return mInternalPipelineStore.get();
    }

    void DeviceBase::IncrementLastSubmittedCommandSerial() {
        mLastSubmittedSerial++;
    }

    void DeviceBase::AssumeCommandsComplete() {
        ExecutionSerial maxSerial =
            ExecutionSerial(std::max(mLastSubmittedSerial + ExecutionSerial(1), mFutureSerial));
        mLastSubmittedSerial = maxSerial;
        mCompletedSerial = maxSerial;
    }

    bool DeviceBase::IsDeviceIdle() {
        if (mAsyncTaskManager->HasPendingTasks()) {
            return false;
        }

        ExecutionSerial maxSerial = std::max(mLastSubmittedSerial, mFutureSerial);
        if (mCompletedSerial == maxSerial) {
            return true;
        }
        return false;
    }

    ExecutionSerial DeviceBase::GetPendingCommandSerial() const {
        return mLastSubmittedSerial + ExecutionSerial(1);
    }

    void DeviceBase::AddFutureSerial(ExecutionSerial serial) {
        if (serial > mFutureSerial) {
            mFutureSerial = serial;
        }
    }

    MaybeError DeviceBase::CheckPassedSerials() {
        ExecutionSerial completedSerial;
        DAWN_TRY_ASSIGN(completedSerial, CheckAndUpdateCompletedSerials());

        ASSERT(completedSerial <= mLastSubmittedSerial);
        // completedSerial should not be less than mCompletedSerial unless it is 0.
        // It can be 0 when there's no fences to check.
        ASSERT(completedSerial >= mCompletedSerial || completedSerial == ExecutionSerial(0));

        if (completedSerial > mCompletedSerial) {
            mCompletedSerial = completedSerial;
        }

        return {};
    }

    ResultOrError<const Format*> DeviceBase::GetInternalFormat(wgpu::TextureFormat format) const {
        size_t index = ComputeFormatIndex(format);
        if (index >= mFormatTable.size()) {
            return DAWN_VALIDATION_ERROR("Unknown texture format");
        }

        const Format* internalFormat = &mFormatTable[index];
        if (!internalFormat->isSupported) {
            return DAWN_VALIDATION_ERROR("Unsupported texture format");
        }

        return internalFormat;
    }

    const Format& DeviceBase::GetValidInternalFormat(wgpu::TextureFormat format) const {
        size_t index = ComputeFormatIndex(format);
        ASSERT(index < mFormatTable.size());
        ASSERT(mFormatTable[index].isSupported);
        return mFormatTable[index];
    }

    ResultOrError<Ref<BindGroupLayoutBase>> DeviceBase::GetOrCreateBindGroupLayout(
        const BindGroupLayoutDescriptor* descriptor,
        PipelineCompatibilityToken pipelineCompatibilityToken) {
        BindGroupLayoutBase blueprint(this, descriptor, pipelineCompatibilityToken);

        const size_t blueprintHash = blueprint.ComputeContentHash();
        blueprint.SetContentHash(blueprintHash);

        Ref<BindGroupLayoutBase> result;
        auto iter = mCaches->bindGroupLayouts.find(&blueprint);
        if (iter != mCaches->bindGroupLayouts.end()) {
            result = *iter;
        } else {
            DAWN_TRY_ASSIGN(result,
                            CreateBindGroupLayoutImpl(descriptor, pipelineCompatibilityToken));
            result->SetIsCachedReference();
            result->SetContentHash(blueprintHash);
            mCaches->bindGroupLayouts.insert(result.Get());
        }

        return std::move(result);
    }

    void DeviceBase::UncacheBindGroupLayout(BindGroupLayoutBase* obj) {
        ASSERT(obj->IsCachedReference());
        size_t removedCount = mCaches->bindGroupLayouts.erase(obj);
        ASSERT(removedCount == 1);
    }

    // Private function used at initialization
    ResultOrError<Ref<BindGroupLayoutBase>> DeviceBase::CreateEmptyBindGroupLayout() {
        BindGroupLayoutDescriptor desc = {};
        desc.entryCount = 0;
        desc.entries = nullptr;

        return GetOrCreateBindGroupLayout(&desc);
    }

    BindGroupLayoutBase* DeviceBase::GetEmptyBindGroupLayout() {
        ASSERT(mEmptyBindGroupLayout != nullptr);
        return mEmptyBindGroupLayout.Get();
    }

    std::pair<Ref<ComputePipelineBase>, size_t> DeviceBase::GetCachedComputePipeline(
        const ComputePipelineDescriptor* descriptor) {
        ComputePipelineBase blueprint(this, descriptor);

        const size_t blueprintHash = blueprint.ComputeContentHash();
        blueprint.SetContentHash(blueprintHash);

        Ref<ComputePipelineBase> result;
        auto iter = mCaches->computePipelines.find(&blueprint);
        if (iter != mCaches->computePipelines.end()) {
            result = *iter;
        }

        return std::make_pair(result, blueprintHash);
    }

    Ref<RenderPipelineBase> DeviceBase::GetCachedRenderPipeline(
        RenderPipelineBase* uninitializedRenderPipeline) {
        Ref<RenderPipelineBase> cachedPipeline;
        auto iter = mCaches->renderPipelines.find(uninitializedRenderPipeline);
        if (iter != mCaches->renderPipelines.end()) {
            cachedPipeline = *iter;
        }
        return cachedPipeline;
    }

    Ref<ComputePipelineBase> DeviceBase::AddOrGetCachedComputePipeline(
        Ref<ComputePipelineBase> computePipeline,
        size_t blueprintHash) {
        computePipeline->SetContentHash(blueprintHash);
        auto insertion = mCaches->computePipelines.insert(computePipeline.Get());
        if (insertion.second) {
            computePipeline->SetIsCachedReference();
            return computePipeline;
        } else {
            return *(insertion.first);
        }
    }

    Ref<RenderPipelineBase> DeviceBase::AddOrGetCachedRenderPipeline(
        Ref<RenderPipelineBase> renderPipeline) {
        auto insertion = mCaches->renderPipelines.insert(renderPipeline.Get());
        if (insertion.second) {
            renderPipeline->SetIsCachedReference();
            return renderPipeline;
        } else {
            return *(insertion.first);
        }
    }

    void DeviceBase::UncacheComputePipeline(ComputePipelineBase* obj) {
        ASSERT(obj->IsCachedReference());
        size_t removedCount = mCaches->computePipelines.erase(obj);
        ASSERT(removedCount == 1);
    }

    ResultOrError<Ref<PipelineLayoutBase>> DeviceBase::GetOrCreatePipelineLayout(
        const PipelineLayoutDescriptor* descriptor) {
        PipelineLayoutBase blueprint(this, descriptor);

        const size_t blueprintHash = blueprint.ComputeContentHash();
        blueprint.SetContentHash(blueprintHash);

        Ref<PipelineLayoutBase> result;
        auto iter = mCaches->pipelineLayouts.find(&blueprint);
        if (iter != mCaches->pipelineLayouts.end()) {
            result = *iter;
        } else {
            DAWN_TRY_ASSIGN(result, CreatePipelineLayoutImpl(descriptor));
            result->SetIsCachedReference();
            result->SetContentHash(blueprintHash);
            mCaches->pipelineLayouts.insert(result.Get());
        }

        return std::move(result);
    }

    void DeviceBase::UncachePipelineLayout(PipelineLayoutBase* obj) {
        ASSERT(obj->IsCachedReference());
        size_t removedCount = mCaches->pipelineLayouts.erase(obj);
        ASSERT(removedCount == 1);
    }

    void DeviceBase::UncacheRenderPipeline(RenderPipelineBase* obj) {
        ASSERT(obj->IsCachedReference());
        size_t removedCount = mCaches->renderPipelines.erase(obj);
        ASSERT(removedCount == 1);
    }

    ResultOrError<Ref<SamplerBase>> DeviceBase::GetOrCreateSampler(
        const SamplerDescriptor* descriptor) {
        SamplerBase blueprint(this, descriptor);

        const size_t blueprintHash = blueprint.ComputeContentHash();
        blueprint.SetContentHash(blueprintHash);

        Ref<SamplerBase> result;
        auto iter = mCaches->samplers.find(&blueprint);
        if (iter != mCaches->samplers.end()) {
            result = *iter;
        } else {
            DAWN_TRY_ASSIGN(result, CreateSamplerImpl(descriptor));
            result->SetIsCachedReference();
            result->SetContentHash(blueprintHash);
            mCaches->samplers.insert(result.Get());
        }

        return std::move(result);
    }

    void DeviceBase::UncacheSampler(SamplerBase* obj) {
        ASSERT(obj->IsCachedReference());
        size_t removedCount = mCaches->samplers.erase(obj);
        ASSERT(removedCount == 1);
    }

    ResultOrError<Ref<ShaderModuleBase>> DeviceBase::GetOrCreateShaderModule(
        const ShaderModuleDescriptor* descriptor,
        ShaderModuleParseResult* parseResult,
        OwnedCompilationMessages* compilationMessages) {
        ASSERT(parseResult != nullptr);

        ShaderModuleBase blueprint(this, descriptor);

        const size_t blueprintHash = blueprint.ComputeContentHash();
        blueprint.SetContentHash(blueprintHash);

        Ref<ShaderModuleBase> result;
        auto iter = mCaches->shaderModules.find(&blueprint);
        if (iter != mCaches->shaderModules.end()) {
            result = *iter;
        } else {
            if (!parseResult->HasParsedShader()) {
                // We skip the parse on creation if validation isn't enabled which let's us quickly
                // lookup in the cache without validating and parsing. We need the parsed module
                // now, so call validate. Most of |ValidateShaderModuleDescriptor| is parsing, but
                // we can consider splitting it if additional validation is added.
                ASSERT(!IsValidationEnabled());
                DAWN_TRY(ValidateShaderModuleDescriptor(this, descriptor, parseResult,
                                                        compilationMessages));
            }
            DAWN_TRY_ASSIGN(result, CreateShaderModuleImpl(descriptor, parseResult));
            result->SetIsCachedReference();
            result->SetContentHash(blueprintHash);
            mCaches->shaderModules.insert(result.Get());
        }

        return std::move(result);
    }

    void DeviceBase::UncacheShaderModule(ShaderModuleBase* obj) {
        ASSERT(obj->IsCachedReference());
        size_t removedCount = mCaches->shaderModules.erase(obj);
        ASSERT(removedCount == 1);
    }

    Ref<AttachmentState> DeviceBase::GetOrCreateAttachmentState(
        AttachmentStateBlueprint* blueprint) {
        auto iter = mCaches->attachmentStates.find(blueprint);
        if (iter != mCaches->attachmentStates.end()) {
            return static_cast<AttachmentState*>(*iter);
        }

        Ref<AttachmentState> attachmentState = AcquireRef(new AttachmentState(this, *blueprint));
        attachmentState->SetIsCachedReference();
        attachmentState->SetContentHash(attachmentState->ComputeContentHash());
        mCaches->attachmentStates.insert(attachmentState.Get());
        return attachmentState;
    }

    Ref<AttachmentState> DeviceBase::GetOrCreateAttachmentState(
        const RenderBundleEncoderDescriptor* descriptor) {
        AttachmentStateBlueprint blueprint(descriptor);
        return GetOrCreateAttachmentState(&blueprint);
    }

    Ref<AttachmentState> DeviceBase::GetOrCreateAttachmentState(
        const RenderPipelineDescriptor* descriptor) {
        AttachmentStateBlueprint blueprint(descriptor);
        return GetOrCreateAttachmentState(&blueprint);
    }

    Ref<AttachmentState> DeviceBase::GetOrCreateAttachmentState(
        const RenderPassDescriptor* descriptor) {
        AttachmentStateBlueprint blueprint(descriptor);
        return GetOrCreateAttachmentState(&blueprint);
    }

    void DeviceBase::UncacheAttachmentState(AttachmentState* obj) {
        ASSERT(obj->IsCachedReference());
        size_t removedCount = mCaches->attachmentStates.erase(obj);
        ASSERT(removedCount == 1);
    }

    // Object creation API methods

    BindGroupBase* DeviceBase::APICreateBindGroup(const BindGroupDescriptor* descriptor) {
        Ref<BindGroupBase> result;
        if (ConsumedError(CreateBindGroup(descriptor), &result, "calling CreateBindGroup(%s).",
                          descriptor)) {
            return BindGroupBase::MakeError(this);
        }
        return result.Detach();
    }
    BindGroupLayoutBase* DeviceBase::APICreateBindGroupLayout(
        const BindGroupLayoutDescriptor* descriptor) {
        Ref<BindGroupLayoutBase> result;
        if (ConsumedError(CreateBindGroupLayout(descriptor), &result,
                          "calling CreateBindGroupLayout(%s).", descriptor)) {
            return BindGroupLayoutBase::MakeError(this);
        }
        return result.Detach();
    }
    BufferBase* DeviceBase::APICreateBuffer(const BufferDescriptor* descriptor) {
        Ref<BufferBase> result = nullptr;
        if (ConsumedError(CreateBuffer(descriptor), &result, "calling CreateBuffer(%s).",
                          descriptor)) {
            ASSERT(result == nullptr);
            return BufferBase::MakeError(this, descriptor);
        }
        return result.Detach();
    }
    CommandEncoder* DeviceBase::APICreateCommandEncoder(
        const CommandEncoderDescriptor* descriptor) {
        return new CommandEncoder(this, descriptor);
    }
    ComputePipelineBase* DeviceBase::APICreateComputePipeline(
        const ComputePipelineDescriptor* descriptor) {
        Ref<ComputePipelineBase> result;
        if (ConsumedError(CreateComputePipeline(descriptor), &result,
                          "calling CreateComputePipeline(%s).", descriptor)) {
            return ComputePipelineBase::MakeError(this);
        }
        return result.Detach();
    }
    void DeviceBase::APICreateComputePipelineAsync(const ComputePipelineDescriptor* descriptor,
                                                   WGPUCreateComputePipelineAsyncCallback callback,
                                                   void* userdata) {
        MaybeError maybeResult = CreateComputePipelineAsync(descriptor, callback, userdata);

        // Call the callback directly when a validation error has been found in the front-end
        // validations. If there is no error, then CreateComputePipelineAsync will call the
        // callback.
        if (maybeResult.IsError()) {
            std::unique_ptr<ErrorData> error = maybeResult.AcquireError();
            callback(WGPUCreatePipelineAsyncStatus_Error, nullptr, error->GetMessage().c_str(),
                     userdata);
        }
    }
    PipelineLayoutBase* DeviceBase::APICreatePipelineLayout(
        const PipelineLayoutDescriptor* descriptor) {
        Ref<PipelineLayoutBase> result;
        if (ConsumedError(CreatePipelineLayout(descriptor), &result,
                          "calling CreatePipelineLayout(%s).", descriptor)) {
            return PipelineLayoutBase::MakeError(this);
        }
        return result.Detach();
    }
    QuerySetBase* DeviceBase::APICreateQuerySet(const QuerySetDescriptor* descriptor) {
        Ref<QuerySetBase> result;
        if (ConsumedError(CreateQuerySet(descriptor), &result, "calling CreateQuerySet(%s).",
                          descriptor)) {
            return QuerySetBase::MakeError(this);
        }
        return result.Detach();
    }
    SamplerBase* DeviceBase::APICreateSampler(const SamplerDescriptor* descriptor) {
        Ref<SamplerBase> result;
        if (ConsumedError(CreateSampler(descriptor), &result, "calling CreateSampler(%s).",
                          descriptor)) {
            return SamplerBase::MakeError(this);
        }
        return result.Detach();
    }
    void DeviceBase::APICreateRenderPipelineAsync(const RenderPipelineDescriptor* descriptor,
                                                  WGPUCreateRenderPipelineAsyncCallback callback,
                                                  void* userdata) {
        // TODO(dawn:563): Add validation error context.
        MaybeError maybeResult = CreateRenderPipelineAsync(descriptor, callback, userdata);

        // Call the callback directly when a validation error has been found in the front-end
        // validations. If there is no error, then CreateRenderPipelineAsync will call the
        // callback.
        if (maybeResult.IsError()) {
            std::unique_ptr<ErrorData> error = maybeResult.AcquireError();
            callback(WGPUCreatePipelineAsyncStatus_Error, nullptr, error->GetMessage().c_str(),
                     userdata);
        }
    }
    RenderBundleEncoder* DeviceBase::APICreateRenderBundleEncoder(
        const RenderBundleEncoderDescriptor* descriptor) {
        Ref<RenderBundleEncoder> result;
        if (ConsumedError(CreateRenderBundleEncoder(descriptor), &result,
                          "calling CreateRenderBundleEncoder(%s).", descriptor)) {
            return RenderBundleEncoder::MakeError(this);
        }
        return result.Detach();
    }
    RenderPipelineBase* DeviceBase::APICreateRenderPipeline(
        const RenderPipelineDescriptor* descriptor) {
        Ref<RenderPipelineBase> result;
        if (ConsumedError(CreateRenderPipeline(descriptor), &result,
                          "calling CreateRenderPipeline(%s).", descriptor)) {
            return RenderPipelineBase::MakeError(this);
        }
        return result.Detach();
    }
    ShaderModuleBase* DeviceBase::APICreateShaderModule(const ShaderModuleDescriptor* descriptor) {
        Ref<ShaderModuleBase> result;
        std::unique_ptr<OwnedCompilationMessages> compilationMessages(
            std::make_unique<OwnedCompilationMessages>());
        if (ConsumedError(CreateShaderModule(descriptor, compilationMessages.get()), &result,
                          "calling CreateShaderModule(%s).", descriptor)) {
            DAWN_ASSERT(result == nullptr);
            result = ShaderModuleBase::MakeError(this);
        }
        // Move compilation messages into ShaderModuleBase and emit tint errors and warnings
        // after all other operations are finished successfully.
        result->InjectCompilationMessages(std::move(compilationMessages));

        return result.Detach();
    }
    SwapChainBase* DeviceBase::APICreateSwapChain(Surface* surface,
                                                  const SwapChainDescriptor* descriptor) {
        Ref<SwapChainBase> result;
        if (ConsumedError(CreateSwapChain(surface, descriptor), &result,
                          "calling CreateSwapChain(%s).", descriptor)) {
            return SwapChainBase::MakeError(this);
        }
        return result.Detach();
    }
    TextureBase* DeviceBase::APICreateTexture(const TextureDescriptor* descriptor) {
        Ref<TextureBase> result;
        if (ConsumedError(CreateTexture(descriptor), &result, "calling CreateTexture(%s).",
                          descriptor)) {
            return TextureBase::MakeError(this);
        }
        return result.Detach();
    }

    // For Dawn Wire

    BufferBase* DeviceBase::APICreateErrorBuffer() {
        BufferDescriptor desc = {};
        return BufferBase::MakeError(this, &desc);
    }

    // Other Device API methods

    // Returns true if future ticking is needed.
    bool DeviceBase::APITick() {
        if (ConsumedError(Tick())) {
            return false;
        }
        return !IsDeviceIdle();
    }

    MaybeError DeviceBase::Tick() {
        DAWN_TRY(ValidateIsAlive());

        // to avoid overly ticking, we only want to tick when:
        // 1. the last submitted serial has moved beyond the completed serial
        // 2. or the completed serial has not reached the future serial set by the trackers
        if (mLastSubmittedSerial > mCompletedSerial || mCompletedSerial < mFutureSerial) {
            DAWN_TRY(CheckPassedSerials());
            DAWN_TRY(TickImpl());

            // There is no GPU work in flight, we need to move the serials forward so that
            // so that CPU operations waiting on GPU completion can know they don't have to wait.
            // AssumeCommandsComplete will assign the max serial we must tick to in order to
            // fire the awaiting callbacks.
            if (mCompletedSerial == mLastSubmittedSerial) {
                AssumeCommandsComplete();
            }

            // TODO(crbug.com/dawn/833): decouple TickImpl from updating the serial so that we can
            // tick the dynamic uploader before the backend resource allocators. This would allow
            // reclaiming resources one tick earlier.
            mDynamicUploader->Deallocate(mCompletedSerial);
            mQueue->Tick(mCompletedSerial);
        }

        // We have to check callback tasks in every Tick because it is not related to any global
        // serials.
        FlushCallbackTaskQueue();

        return {};
    }

    QueueBase* DeviceBase::APIGetQueue() {
        // Backends gave the primary queue during initialization.
        ASSERT(mQueue != nullptr);

        // Returns a new reference to the queue.
        mQueue->Reference();
        return mQueue.Get();
    }

    ExternalTextureBase* DeviceBase::APICreateExternalTexture(
        const ExternalTextureDescriptor* descriptor) {
        Ref<ExternalTextureBase> result = nullptr;
        if (ConsumedError(CreateExternalTexture(descriptor), &result,
                          "calling CreateExternalTexture(%s).", descriptor)) {
            return ExternalTextureBase::MakeError(this);
        }

        return result.Detach();
    }

    void DeviceBase::ApplyFeatures(const DeviceDescriptor* deviceDescriptor) {
        ASSERT(deviceDescriptor);
        // TODO(dawn:1149): remove once requiredExtensions is no longer used.
        ASSERT(GetAdapter()->SupportsAllRequestedFeatures(deviceDescriptor->requiredExtensions));
        ASSERT(GetAdapter()->SupportsAllRequestedFeatures(deviceDescriptor->requiredFeatures));

        // TODO(dawn:1149): remove once requiredExtensions is no longer used.
        mEnabledExtensions = GetAdapter()->GetInstance()->FeatureNamesToFeaturesSet(
            deviceDescriptor->requiredExtensions);
        mEnabledFeatures = GetAdapter()->GetInstance()->FeatureNamesToFeaturesSet(
            deviceDescriptor->requiredFeatures);
    }

    std::vector<const char*> DeviceBase::GetEnabledFeatures() const {
        return mEnabledFeatures.GetEnabledFeatureNames();
    }

    bool DeviceBase::IsFeatureEnabled(Feature feature) const {
        // TODO(dawn:1149): remove mEnabledExtensions once it is no longer used.
        return mEnabledFeatures.IsEnabled(feature) || mEnabledExtensions.IsEnabled(feature);
    }

    bool DeviceBase::IsValidationEnabled() const {
        return !IsToggleEnabled(Toggle::SkipValidation);
    }

    bool DeviceBase::IsRobustnessEnabled() const {
        return !IsToggleEnabled(Toggle::DisableRobustness);
    }

    size_t DeviceBase::GetLazyClearCountForTesting() {
        return mLazyClearCountForTesting;
    }

    void DeviceBase::IncrementLazyClearCountForTesting() {
        ++mLazyClearCountForTesting;
    }

    size_t DeviceBase::GetDeprecationWarningCountForTesting() {
        return mDeprecationWarnings->count;
    }

    void DeviceBase::EmitDeprecationWarning(const char* warning) {
        mDeprecationWarnings->count++;
        if (mDeprecationWarnings->emitted.insert(warning).second) {
            dawn::WarningLog() << warning;
        }
    }

    void DeviceBase::EmitLog(const char* message) {
        this->EmitLog(WGPULoggingType_Info, message);
    }

    void DeviceBase::EmitLog(WGPULoggingType loggingType, const char* message) {
        if (mLoggingCallback != nullptr) {
            // Use the thread-safe CallbackTaskManager routine
            std::unique_ptr<LoggingCallbackTask> callbackTask =
                std::make_unique<LoggingCallbackTask>(mLoggingCallback, loggingType, message,
                                                      mLoggingUserdata);
            mCallbackTaskManager->AddCallbackTask(std::move(callbackTask));
        }
    }

    bool DeviceBase::APIGetLimits(SupportedLimits* limits) {
        ASSERT(limits != nullptr);
        if (limits->nextInChain != nullptr) {
            return false;
        }
        limits->limits = mLimits.v1;
        return true;
    }

    void DeviceBase::APIInjectError(wgpu::ErrorType type, const char* message) {
        if (ConsumedError(ValidateErrorType(type))) {
            return;
        }

        // This method should only be used to make error scope reject. For DeviceLost there is the
        // LoseForTesting function that can be used instead.
        if (type != wgpu::ErrorType::Validation && type != wgpu::ErrorType::OutOfMemory) {
            HandleError(InternalErrorType::Validation,
                        "Invalid injected error, must be Validation or OutOfMemory");
            return;
        }

        HandleError(FromWGPUErrorType(type), message);
    }

    QueueBase* DeviceBase::GetQueue() const {
        return mQueue.Get();
    }

    // Implementation details of object creation

    ResultOrError<Ref<BindGroupBase>> DeviceBase::CreateBindGroup(
        const BindGroupDescriptor* descriptor) {
        DAWN_TRY(ValidateIsAlive());
        if (IsValidationEnabled()) {
            DAWN_TRY_CONTEXT(ValidateBindGroupDescriptor(this, descriptor),
                             "validating %s against %s", descriptor, descriptor->layout);
        }
        return CreateBindGroupImpl(descriptor);
    }

    ResultOrError<Ref<BindGroupLayoutBase>> DeviceBase::CreateBindGroupLayout(
        const BindGroupLayoutDescriptor* descriptor,
        bool allowInternalBinding) {
        DAWN_TRY(ValidateIsAlive());
        if (IsValidationEnabled()) {
            DAWN_TRY(ValidateBindGroupLayoutDescriptor(this, descriptor, allowInternalBinding));
        }
        return GetOrCreateBindGroupLayout(descriptor);
    }

    ResultOrError<Ref<BufferBase>> DeviceBase::CreateBuffer(const BufferDescriptor* descriptor) {
        DAWN_TRY(ValidateIsAlive());
        if (IsValidationEnabled()) {
            DAWN_TRY_CONTEXT(ValidateBufferDescriptor(this, descriptor), "validating %s",
                             descriptor);
        }

        Ref<BufferBase> buffer;
        DAWN_TRY_ASSIGN(buffer, CreateBufferImpl(descriptor));

        if (descriptor->mappedAtCreation) {
            DAWN_TRY(buffer->MapAtCreation());
        }

        return std::move(buffer);
    }

    ResultOrError<Ref<ComputePipelineBase>> DeviceBase::CreateComputePipeline(
        const ComputePipelineDescriptor* descriptor) {
        DAWN_TRY(ValidateIsAlive());
        if (IsValidationEnabled()) {
            DAWN_TRY(ValidateComputePipelineDescriptor(this, descriptor));
        }

        // Ref will keep the pipeline layout alive until the end of the function where
        // the pipeline will take another reference.
        Ref<PipelineLayoutBase> layoutRef;
        ComputePipelineDescriptor appliedDescriptor;
        DAWN_TRY_ASSIGN(layoutRef, ValidateLayoutAndGetComputePipelineDescriptorWithDefaults(
                                       this, *descriptor, &appliedDescriptor));

        auto pipelineAndBlueprintFromCache = GetCachedComputePipeline(&appliedDescriptor);
        if (pipelineAndBlueprintFromCache.first.Get() != nullptr) {
            return std::move(pipelineAndBlueprintFromCache.first);
        }

        Ref<ComputePipelineBase> backendObj;
        DAWN_TRY_ASSIGN(backendObj, CreateComputePipelineImpl(&appliedDescriptor));
        size_t blueprintHash = pipelineAndBlueprintFromCache.second;
        return AddOrGetCachedComputePipeline(backendObj, blueprintHash);
    }

    MaybeError DeviceBase::CreateComputePipelineAsync(
        const ComputePipelineDescriptor* descriptor,
        WGPUCreateComputePipelineAsyncCallback callback,
        void* userdata) {
        DAWN_TRY(ValidateIsAlive());
        if (IsValidationEnabled()) {
            DAWN_TRY(ValidateComputePipelineDescriptor(this, descriptor));
        }

        Ref<PipelineLayoutBase> layoutRef;
        ComputePipelineDescriptor appliedDescriptor;
        DAWN_TRY_ASSIGN(layoutRef, ValidateLayoutAndGetComputePipelineDescriptorWithDefaults(
                                       this, *descriptor, &appliedDescriptor));

        // Call the callback directly when we can get a cached compute pipeline object.
        auto pipelineAndBlueprintFromCache = GetCachedComputePipeline(&appliedDescriptor);
        if (pipelineAndBlueprintFromCache.first.Get() != nullptr) {
            Ref<ComputePipelineBase> result = std::move(pipelineAndBlueprintFromCache.first);
            callback(WGPUCreatePipelineAsyncStatus_Success,
                     reinterpret_cast<WGPUComputePipeline>(result.Detach()), "", userdata);
        } else {
            // Otherwise we will create the pipeline object in CreateComputePipelineAsyncImpl(),
            // where the pipeline object may be created asynchronously and the result will be saved
            // to mCreatePipelineAsyncTracker.
            const size_t blueprintHash = pipelineAndBlueprintFromCache.second;
            CreateComputePipelineAsyncImpl(&appliedDescriptor, blueprintHash, callback, userdata);
        }

        return {};
    }

    // This function is overwritten with the async version on the backends that supports creating
    // compute pipeline asynchronously.
    void DeviceBase::CreateComputePipelineAsyncImpl(const ComputePipelineDescriptor* descriptor,
                                                    size_t blueprintHash,
                                                    WGPUCreateComputePipelineAsyncCallback callback,
                                                    void* userdata) {
        Ref<ComputePipelineBase> result;
        std::string errorMessage;

        auto resultOrError = CreateComputePipelineImpl(descriptor);
        if (resultOrError.IsError()) {
            std::unique_ptr<ErrorData> error = resultOrError.AcquireError();
            errorMessage = error->GetMessage();
        } else {
            result = AddOrGetCachedComputePipeline(resultOrError.AcquireSuccess(), blueprintHash);
        }

        std::unique_ptr<CreateComputePipelineAsyncCallbackTask> callbackTask =
            std::make_unique<CreateComputePipelineAsyncCallbackTask>(
                std::move(result), errorMessage, callback, userdata);
        mCallbackTaskManager->AddCallbackTask(std::move(callbackTask));
    }

    // This function is overwritten with the async version on the backends
    // that supports initializing render pipeline asynchronously
    void DeviceBase::InitializeRenderPipelineAsyncImpl(
        Ref<RenderPipelineBase> renderPipeline,
        WGPUCreateRenderPipelineAsyncCallback callback,
        void* userdata) {
        Ref<RenderPipelineBase> result;
        std::string errorMessage;

        MaybeError maybeError = renderPipeline->Initialize();
        if (maybeError.IsError()) {
            std::unique_ptr<ErrorData> error = maybeError.AcquireError();
            errorMessage = error->GetMessage();
        } else {
            result = AddOrGetCachedRenderPipeline(renderPipeline);
        }

        std::unique_ptr<CreateRenderPipelineAsyncCallbackTask> callbackTask =
            std::make_unique<CreateRenderPipelineAsyncCallbackTask>(std::move(result), errorMessage,
                                                                    callback, userdata);
        mCallbackTaskManager->AddCallbackTask(std::move(callbackTask));
    }

    ResultOrError<Ref<PipelineLayoutBase>> DeviceBase::CreatePipelineLayout(
        const PipelineLayoutDescriptor* descriptor) {
        DAWN_TRY(ValidateIsAlive());
        if (IsValidationEnabled()) {
            DAWN_TRY(ValidatePipelineLayoutDescriptor(this, descriptor));
        }
        return GetOrCreatePipelineLayout(descriptor);
    }

    ResultOrError<Ref<ExternalTextureBase>> DeviceBase::CreateExternalTexture(
        const ExternalTextureDescriptor* descriptor) {
        if (IsValidationEnabled()) {
            DAWN_TRY_CONTEXT(ValidateExternalTextureDescriptor(this, descriptor), "validating %s",
                             descriptor);
        }

        return ExternalTextureBase::Create(this, descriptor);
    }

    ResultOrError<Ref<QuerySetBase>> DeviceBase::CreateQuerySet(
        const QuerySetDescriptor* descriptor) {
        DAWN_TRY(ValidateIsAlive());
        if (IsValidationEnabled()) {
            DAWN_TRY_CONTEXT(ValidateQuerySetDescriptor(this, descriptor), "validating %s",
                             descriptor);
        }
        return CreateQuerySetImpl(descriptor);
    }

    ResultOrError<Ref<RenderBundleEncoder>> DeviceBase::CreateRenderBundleEncoder(
        const RenderBundleEncoderDescriptor* descriptor) {
        DAWN_TRY(ValidateIsAlive());
        if (IsValidationEnabled()) {
            DAWN_TRY(ValidateRenderBundleEncoderDescriptor(this, descriptor));
        }
        return RenderBundleEncoder::Create(this, descriptor);
    }

    ResultOrError<Ref<RenderPipelineBase>> DeviceBase::CreateRenderPipeline(
        const RenderPipelineDescriptor* descriptor) {
        DAWN_TRY(ValidateIsAlive());
        if (IsValidationEnabled()) {
            DAWN_TRY(ValidateRenderPipelineDescriptor(this, descriptor));
        }

        // Ref will keep the pipeline layout alive until the end of the function where
        // the pipeline will take another reference.
        Ref<PipelineLayoutBase> layoutRef;
        RenderPipelineDescriptor appliedDescriptor;
        DAWN_TRY_ASSIGN(layoutRef, ValidateLayoutAndGetRenderPipelineDescriptorWithDefaults(
                                       this, *descriptor, &appliedDescriptor));

        Ref<RenderPipelineBase> uninitializedRenderPipeline =
            CreateUninitializedRenderPipeline(&appliedDescriptor);

        Ref<RenderPipelineBase> cachedRenderPipeline =
            GetCachedRenderPipeline(uninitializedRenderPipeline.Get());
        if (cachedRenderPipeline != nullptr) {
            return cachedRenderPipeline;
        }

        DAWN_TRY(uninitializedRenderPipeline->Initialize());
        return AddOrGetCachedRenderPipeline(std::move(uninitializedRenderPipeline));
    }

    MaybeError DeviceBase::CreateRenderPipelineAsync(const RenderPipelineDescriptor* descriptor,
                                                     WGPUCreateRenderPipelineAsyncCallback callback,
                                                     void* userdata) {
        DAWN_TRY(ValidateIsAlive());
        if (IsValidationEnabled()) {
            DAWN_TRY(ValidateRenderPipelineDescriptor(this, descriptor));
        }

        // Ref will keep the pipeline layout alive until the end of the function where
        // the pipeline will take another reference.
        Ref<PipelineLayoutBase> layoutRef;
        RenderPipelineDescriptor appliedDescriptor;
        DAWN_TRY_ASSIGN(layoutRef, ValidateLayoutAndGetRenderPipelineDescriptorWithDefaults(
                                       this, *descriptor, &appliedDescriptor));

        Ref<RenderPipelineBase> uninitializedRenderPipeline =
            CreateUninitializedRenderPipeline(&appliedDescriptor);

        // Call the callback directly when we can get a cached render pipeline object.
        Ref<RenderPipelineBase> cachedRenderPipeline =
            GetCachedRenderPipeline(uninitializedRenderPipeline.Get());
        if (cachedRenderPipeline != nullptr) {
            callback(WGPUCreatePipelineAsyncStatus_Success,
                     reinterpret_cast<WGPURenderPipeline>(cachedRenderPipeline.Detach()), "",
                     userdata);
        } else {
            // Otherwise we will create the pipeline object in InitializeRenderPipelineAsyncImpl(),
            // where the pipeline object may be initialized asynchronously and the result will be
            // saved to mCreatePipelineAsyncTracker.
            InitializeRenderPipelineAsyncImpl(std::move(uninitializedRenderPipeline), callback,
                                              userdata);
        }

        return {};
    }

    ResultOrError<Ref<SamplerBase>> DeviceBase::CreateSampler(const SamplerDescriptor* descriptor) {
        const SamplerDescriptor defaultDescriptor = {};
        DAWN_TRY(ValidateIsAlive());
        descriptor = descriptor != nullptr ? descriptor : &defaultDescriptor;
        if (IsValidationEnabled()) {
            DAWN_TRY_CONTEXT(ValidateSamplerDescriptor(this, descriptor), "validating %s",
                             descriptor);
        }
        return GetOrCreateSampler(descriptor);
    }

    ResultOrError<Ref<ShaderModuleBase>> DeviceBase::CreateShaderModule(
        const ShaderModuleDescriptor* descriptor,
        OwnedCompilationMessages* compilationMessages) {
        DAWN_TRY(ValidateIsAlive());

        // CreateShaderModule can be called from inside dawn_native. If that's the case handle the
        // error directly in Dawn and no compilationMessages held in the shader module. It is ok as
        // long as dawn_native don't use the compilationMessages of these internal shader modules.
        ShaderModuleParseResult parseResult;

        if (IsValidationEnabled()) {
            DAWN_TRY(ValidateShaderModuleDescriptor(this, descriptor, &parseResult,
                                                    compilationMessages));
        }

        return GetOrCreateShaderModule(descriptor, &parseResult, compilationMessages);
    }

    ResultOrError<Ref<SwapChainBase>> DeviceBase::CreateSwapChain(
        Surface* surface,
        const SwapChainDescriptor* descriptor) {
        DAWN_TRY(ValidateIsAlive());
        if (IsValidationEnabled()) {
            DAWN_TRY(ValidateSwapChainDescriptor(this, surface, descriptor));
        }

        // TODO(dawn:269): Remove this code path once implementation-based swapchains are removed.
        if (surface == nullptr) {
            return CreateSwapChainImpl(descriptor);
        } else {
            ASSERT(descriptor->implementation == 0);

            NewSwapChainBase* previousSwapChain = surface->GetAttachedSwapChain();
            ResultOrError<Ref<NewSwapChainBase>> maybeNewSwapChain =
                CreateSwapChainImpl(surface, previousSwapChain, descriptor);

            if (previousSwapChain != nullptr) {
                previousSwapChain->DetachFromSurface();
            }

            Ref<NewSwapChainBase> newSwapChain;
            DAWN_TRY_ASSIGN(newSwapChain, std::move(maybeNewSwapChain));

            newSwapChain->SetIsAttached();
            surface->SetAttachedSwapChain(newSwapChain.Get());
            return newSwapChain;
        }
    }

    ResultOrError<Ref<TextureBase>> DeviceBase::CreateTexture(const TextureDescriptor* descriptor) {
        DAWN_TRY(ValidateIsAlive());
        if (IsValidationEnabled()) {
            DAWN_TRY_CONTEXT(ValidateTextureDescriptor(this, descriptor), "validating %s.",
                             descriptor);
        }
        return CreateTextureImpl(descriptor);
    }

    ResultOrError<Ref<TextureViewBase>> DeviceBase::CreateTextureView(
        TextureBase* texture,
        const TextureViewDescriptor* descriptor) {
        DAWN_TRY(ValidateIsAlive());
        DAWN_TRY(ValidateObject(texture));
        TextureViewDescriptor desc = GetTextureViewDescriptorWithDefaults(texture, descriptor);
        if (IsValidationEnabled()) {
            DAWN_TRY_CONTEXT(ValidateTextureViewDescriptor(this, texture, &desc),
                             "validating %s against %s.", &desc, texture);
        }
        return CreateTextureViewImpl(texture, &desc);
    }

    // Other implementation details

    DynamicUploader* DeviceBase::GetDynamicUploader() const {
        return mDynamicUploader.get();
    }

    // The Toggle device facility

    std::vector<const char*> DeviceBase::GetTogglesUsed() const {
        return mEnabledToggles.GetContainedToggleNames();
    }

    bool DeviceBase::IsToggleEnabled(Toggle toggle) const {
        return mEnabledToggles.Has(toggle);
    }

    void DeviceBase::SetToggle(Toggle toggle, bool isEnabled) {
        if (!mOverridenToggles.Has(toggle)) {
            mEnabledToggles.Set(toggle, isEnabled);
        }
    }

    void DeviceBase::ForceSetToggle(Toggle toggle, bool isEnabled) {
        if (!mOverridenToggles.Has(toggle) && mEnabledToggles.Has(toggle) != isEnabled) {
            dawn::WarningLog() << "Forcing toggle \"" << ToggleEnumToName(toggle) << "\" to "
                               << isEnabled << " when it was overriden to be " << !isEnabled;
        }
        mEnabledToggles.Set(toggle, isEnabled);
    }

    void DeviceBase::SetDefaultToggles() {
        SetToggle(Toggle::LazyClearResourceOnFirstUse, true);
        SetToggle(Toggle::DisallowUnsafeAPIs, true);
        SetToggle(Toggle::UseTintGenerator, true);
    }

    void DeviceBase::ApplyToggleOverrides(const DeviceDescriptor* deviceDescriptor) {
        ASSERT(deviceDescriptor);

        for (const char* toggleName : deviceDescriptor->forceEnabledToggles) {
            Toggle toggle = GetAdapter()->GetInstance()->ToggleNameToEnum(toggleName);
            if (toggle != Toggle::InvalidEnum) {
                mEnabledToggles.Set(toggle, true);
                mOverridenToggles.Set(toggle, true);
            }
        }
        for (const char* toggleName : deviceDescriptor->forceDisabledToggles) {
            Toggle toggle = GetAdapter()->GetInstance()->ToggleNameToEnum(toggleName);
            if (toggle != Toggle::InvalidEnum) {
                mEnabledToggles.Set(toggle, false);
                mOverridenToggles.Set(toggle, true);
            }
        }
    }

    void DeviceBase::FlushCallbackTaskQueue() {
        if (!mCallbackTaskManager->IsEmpty()) {
            // If a user calls Queue::Submit inside the callback, then the device will be ticked,
            // which in turns ticks the tracker, causing reentrance and dead lock here. To prevent
            // such reentrant call, we remove all the callback tasks from mCallbackTaskManager,
            // update mCallbackTaskManager, then call all the callbacks.
            auto callbackTasks = mCallbackTaskManager->AcquireCallbackTasks();
            for (std::unique_ptr<CallbackTask>& callbackTask : callbackTasks) {
                callbackTask->Finish();
            }
        }
    }

    AsyncTaskManager* DeviceBase::GetAsyncTaskManager() const {
        return mAsyncTaskManager.get();
    }

    CallbackTaskManager* DeviceBase::GetCallbackTaskManager() const {
        return mCallbackTaskManager.get();
    }

    dawn_platform::WorkerTaskPool* DeviceBase::GetWorkerTaskPool() const {
        return mWorkerTaskPool.get();
    }

    Ref<RenderPipelineBase> DeviceBase::CreateUninitializedRenderPipeline(
        const RenderPipelineDescriptor* descriptor) {
        return CreateUninitializedRenderPipelineImpl(descriptor);
    }

    void DeviceBase::AddComputePipelineAsyncCallbackTask(
        Ref<ComputePipelineBase> pipeline,
        std::string errorMessage,
        WGPUCreateComputePipelineAsyncCallback callback,
        void* userdata,
        size_t blueprintHash) {
        // CreateComputePipelineAsyncWaitableCallbackTask is declared as an internal class as it
        // needs to call the private member function DeviceBase::AddOrGetCachedComputePipeline().
        struct CreateComputePipelineAsyncWaitableCallbackTask final
            : CreateComputePipelineAsyncCallbackTask {
            CreateComputePipelineAsyncWaitableCallbackTask(
                Ref<ComputePipelineBase> pipeline,
                std::string errorMessage,
                WGPUCreateComputePipelineAsyncCallback callback,
                void* userdata,
                size_t blueprintHash)
                : CreateComputePipelineAsyncCallbackTask(std::move(pipeline),
                                                         errorMessage,
                                                         callback,
                                                         userdata),
                  mBlueprintHash(blueprintHash) {
            }

            void Finish() final {
                // TODO(dawn:529): call AddOrGetCachedComputePipeline() asynchronously in
                // CreateComputePipelineAsyncTaskImpl::Run() when the front-end pipeline cache is
                // thread-safe.
                if (mPipeline.Get() != nullptr) {
                    mPipeline = mPipeline->GetDevice()->AddOrGetCachedComputePipeline(
                        mPipeline, mBlueprintHash);
                }

                CreateComputePipelineAsyncCallbackTask::Finish();
            }

          private:
            size_t mBlueprintHash;
        };

        mCallbackTaskManager->AddCallbackTask(
            std::make_unique<CreateComputePipelineAsyncWaitableCallbackTask>(
                std::move(pipeline), errorMessage, callback, userdata, blueprintHash));
    }

    void DeviceBase::AddRenderPipelineAsyncCallbackTask(
        Ref<RenderPipelineBase> pipeline,
        std::string errorMessage,
        WGPUCreateRenderPipelineAsyncCallback callback,
        void* userdata) {
        // CreateRenderPipelineAsyncWaitableCallbackTask is declared as an internal class as it
        // needs to call the private member function DeviceBase::AddOrGetCachedRenderPipeline().
        struct CreateRenderPipelineAsyncWaitableCallbackTask final
            : CreateRenderPipelineAsyncCallbackTask {
            using CreateRenderPipelineAsyncCallbackTask::CreateRenderPipelineAsyncCallbackTask;

            void Finish() final {
                // TODO(dawn:529): call AddOrGetCachedRenderPipeline() asynchronously in
                // CreateRenderPipelineAsyncTaskImpl::Run() when the front-end pipeline cache is
                // thread-safe.
                if (mPipeline.Get() != nullptr) {
                    mPipeline = mPipeline->GetDevice()->AddOrGetCachedRenderPipeline(mPipeline);
                }

                CreateRenderPipelineAsyncCallbackTask::Finish();
            }
        };

        mCallbackTaskManager->AddCallbackTask(
            std::make_unique<CreateRenderPipelineAsyncWaitableCallbackTask>(
                std::move(pipeline), errorMessage, callback, userdata));
    }

    PipelineCompatibilityToken DeviceBase::GetNextPipelineCompatibilityToken() {
        return PipelineCompatibilityToken(mNextPipelineCompatibilityToken++);
    }

    const std::string& DeviceBase::GetLabel() const {
        return mLabel;
    }

    void DeviceBase::APISetLabel(const char* label) {
        mLabel = label;
        SetLabelImpl();
    }

    void DeviceBase::SetLabelImpl() {
    }

}  // namespace dawn_native
