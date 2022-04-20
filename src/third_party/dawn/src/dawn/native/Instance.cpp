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

#include "dawn/native/Instance.h"

#include "dawn/common/Assert.h"
#include "dawn/common/GPUInfo.h"
#include "dawn/common/Log.h"
#include "dawn/common/SystemUtils.h"
#include "dawn/native/ChainUtils_autogen.h"
#include "dawn/native/ErrorData.h"
#include "dawn/native/Surface.h"
#include "dawn/native/ValidationUtils_autogen.h"
#include "dawn/platform/DawnPlatform.h"

// For SwiftShader fallback
#if defined(DAWN_ENABLE_BACKEND_VULKAN)
#    include "dawn/native/VulkanBackend.h"
#endif  // defined(DAWN_ENABLE_BACKEND_VULKAN)

#if defined(DAWN_USE_X11)
#    include "dawn/native/XlibXcbFunctions.h"
#endif  // defined(DAWN_USE_X11)

#include <optional>

namespace dawn::native {

    // Forward definitions of each backend's "Connect" function that creates new BackendConnection.
    // Conditionally compiled declarations are used to avoid using static constructors instead.
#if defined(DAWN_ENABLE_BACKEND_D3D12)
    namespace d3d12 {
        BackendConnection* Connect(InstanceBase* instance);
    }
#endif  // defined(DAWN_ENABLE_BACKEND_D3D12)
#if defined(DAWN_ENABLE_BACKEND_METAL)
    namespace metal {
        BackendConnection* Connect(InstanceBase* instance);
    }
#endif  // defined(DAWN_ENABLE_BACKEND_METAL)
#if defined(DAWN_ENABLE_BACKEND_NULL)
    namespace null {
        BackendConnection* Connect(InstanceBase* instance);
    }
#endif  // defined(DAWN_ENABLE_BACKEND_NULL)
#if defined(DAWN_ENABLE_BACKEND_OPENGL)
    namespace opengl {
        BackendConnection* Connect(InstanceBase* instance, wgpu::BackendType backendType);
    }
#endif  // defined(DAWN_ENABLE_BACKEND_OPENGL)
#if defined(DAWN_ENABLE_BACKEND_VULKAN)
    namespace vulkan {
        BackendConnection* Connect(InstanceBase* instance);
    }
#endif  // defined(DAWN_ENABLE_BACKEND_VULKAN)

    namespace {

        BackendsBitset GetEnabledBackends() {
            BackendsBitset enabledBackends;
#if defined(DAWN_ENABLE_BACKEND_NULL)
            enabledBackends.set(wgpu::BackendType::Null);
#endif  // defined(DAWN_ENABLE_BACKEND_NULL)
#if defined(DAWN_ENABLE_BACKEND_D3D12)
            enabledBackends.set(wgpu::BackendType::D3D12);
#endif  // defined(DAWN_ENABLE_BACKEND_D3D12)
#if defined(DAWN_ENABLE_BACKEND_METAL)
            enabledBackends.set(wgpu::BackendType::Metal);
#endif  // defined(DAWN_ENABLE_BACKEND_METAL)
#if defined(DAWN_ENABLE_BACKEND_VULKAN)
            enabledBackends.set(wgpu::BackendType::Vulkan);
#endif  // defined(DAWN_ENABLE_BACKEND_VULKAN)
#if defined(DAWN_ENABLE_BACKEND_DESKTOP_GL)
            enabledBackends.set(wgpu::BackendType::OpenGL);
#endif  // defined(DAWN_ENABLE_BACKEND_DESKTOP_GL)
#if defined(DAWN_ENABLE_BACKEND_OPENGLES)
            enabledBackends.set(wgpu::BackendType::OpenGLES);
#endif  // defined(DAWN_ENABLE_BACKEND_OPENGLES)

            return enabledBackends;
        }

    }  // anonymous namespace

    InstanceBase* APICreateInstance(const InstanceDescriptor* descriptor) {
        return InstanceBase::Create().Detach();
    }

    // InstanceBase

    // static
    Ref<InstanceBase> InstanceBase::Create(const InstanceDescriptor* descriptor) {
        Ref<InstanceBase> instance = AcquireRef(new InstanceBase);
        static constexpr InstanceDescriptor kDefaultDesc = {};
        if (descriptor == nullptr) {
            descriptor = &kDefaultDesc;
        }
        if (instance->ConsumedError(instance->Initialize(descriptor))) {
            return nullptr;
        }
        return instance;
    }

    // TODO(crbug.com/dawn/832): make the platform an initialization parameter of the instance.
    MaybeError InstanceBase::Initialize(const InstanceDescriptor* descriptor) {
        DAWN_TRY(ValidateSingleSType(descriptor->nextInChain, wgpu::SType::DawnInstanceDescriptor));
        const DawnInstanceDescriptor* dawnDesc = nullptr;
        FindInChain(descriptor->nextInChain, &dawnDesc);
        if (dawnDesc != nullptr) {
            for (uint32_t i = 0; i < dawnDesc->additionalRuntimeSearchPathsCount; ++i) {
                mRuntimeSearchPaths.push_back(dawnDesc->additionalRuntimeSearchPaths[i]);
            }
        }
        // Default paths to search are next to the shared library, next to the executable, and
        // no path (just libvulkan.so).
        if (auto p = GetModuleDirectory()) {
            mRuntimeSearchPaths.push_back(std::move(*p));
        }
        if (auto p = GetExecutableDirectory()) {
            mRuntimeSearchPaths.push_back(std::move(*p));
        }
        mRuntimeSearchPaths.push_back("");
        return {};
    }

    void InstanceBase::APIRequestAdapter(const RequestAdapterOptions* options,
                                         WGPURequestAdapterCallback callback,
                                         void* userdata) {
        static constexpr RequestAdapterOptions kDefaultOptions = {};
        if (options == nullptr) {
            options = &kDefaultOptions;
        }
        auto result = RequestAdapterInternal(options);
        if (result.IsError()) {
            auto err = result.AcquireError();
            std::string msg = err->GetFormattedMessage();
            // TODO(crbug.com/dawn/1122): Call callbacks only on wgpuInstanceProcessEvents
            callback(WGPURequestAdapterStatus_Error, nullptr, msg.c_str(), userdata);
        } else {
            Ref<AdapterBase> adapter = result.AcquireSuccess();
            // TODO(crbug.com/dawn/1122): Call callbacks only on wgpuInstanceProcessEvents
            callback(WGPURequestAdapterStatus_Success, ToAPI(adapter.Detach()), nullptr, userdata);
        }
    }

    ResultOrError<Ref<AdapterBase>> InstanceBase::RequestAdapterInternal(
        const RequestAdapterOptions* options) {
        ASSERT(options != nullptr);
        if (options->forceFallbackAdapter) {
#if defined(DAWN_ENABLE_BACKEND_VULKAN)
            if (GetEnabledBackends()[wgpu::BackendType::Vulkan]) {
                dawn_native::vulkan::AdapterDiscoveryOptions vulkanOptions;
                vulkanOptions.forceSwiftShader = true;
                DAWN_TRY(DiscoverAdaptersInternal(&vulkanOptions));
            }
#else
            return Ref<AdapterBase>(nullptr);
#endif  // defined(DAWN_ENABLE_BACKEND_VULKAN)
        } else {
            DiscoverDefaultAdapters();
        }

        wgpu::AdapterType preferredType;
        switch (options->powerPreference) {
            case wgpu::PowerPreference::LowPower:
                preferredType = wgpu::AdapterType::IntegratedGPU;
                break;
            case wgpu::PowerPreference::Undefined:
            case wgpu::PowerPreference::HighPerformance:
                preferredType = wgpu::AdapterType::DiscreteGPU;
                break;
        }

        std::optional<size_t> discreteGPUAdapterIndex;
        std::optional<size_t> integratedGPUAdapterIndex;
        std::optional<size_t> cpuAdapterIndex;
        std::optional<size_t> unknownAdapterIndex;

        for (size_t i = 0; i < mAdapters.size(); ++i) {
            AdapterProperties properties;
            mAdapters[i]->APIGetProperties(&properties);

            if (options->forceFallbackAdapter) {
                if (!gpu_info::IsSwiftshader(properties.vendorID, properties.deviceID)) {
                    continue;
                }
                return mAdapters[i];
            }
            if (properties.adapterType == preferredType) {
                return mAdapters[i];
            }
            switch (properties.adapterType) {
                case wgpu::AdapterType::DiscreteGPU:
                    discreteGPUAdapterIndex = i;
                    break;
                case wgpu::AdapterType::IntegratedGPU:
                    integratedGPUAdapterIndex = i;
                    break;
                case wgpu::AdapterType::CPU:
                    cpuAdapterIndex = i;
                    break;
                case wgpu::AdapterType::Unknown:
                    unknownAdapterIndex = i;
                    break;
            }
        }

        // For now, we always prefer the discrete GPU
        if (discreteGPUAdapterIndex) {
            return mAdapters[*discreteGPUAdapterIndex];
        }
        if (integratedGPUAdapterIndex) {
            return mAdapters[*integratedGPUAdapterIndex];
        }
        if (cpuAdapterIndex) {
            return mAdapters[*cpuAdapterIndex];
        }
        if (unknownAdapterIndex) {
            return mAdapters[*unknownAdapterIndex];
        }

        return Ref<AdapterBase>(nullptr);
    }

    void InstanceBase::DiscoverDefaultAdapters() {
        for (wgpu::BackendType b : IterateBitSet(GetEnabledBackends())) {
            EnsureBackendConnection(b);
        }

        if (mDiscoveredDefaultAdapters) {
            return;
        }

        // Query and merge all default adapters for all backends
        for (std::unique_ptr<BackendConnection>& backend : mBackends) {
            std::vector<Ref<AdapterBase>> backendAdapters = backend->DiscoverDefaultAdapters();

            for (Ref<AdapterBase>& adapter : backendAdapters) {
                ASSERT(adapter->GetBackendType() == backend->GetType());
                ASSERT(adapter->GetInstance() == this);
                mAdapters.push_back(std::move(adapter));
            }
        }

        mDiscoveredDefaultAdapters = true;
    }

    // This is just a wrapper around the real logic that uses Error.h error handling.
    bool InstanceBase::DiscoverAdapters(const AdapterDiscoveryOptionsBase* options) {
        return !ConsumedError(DiscoverAdaptersInternal(options));
    }

    const ToggleInfo* InstanceBase::GetToggleInfo(const char* toggleName) {
        return mTogglesInfo.GetToggleInfo(toggleName);
    }

    Toggle InstanceBase::ToggleNameToEnum(const char* toggleName) {
        return mTogglesInfo.ToggleNameToEnum(toggleName);
    }

    const FeatureInfo* InstanceBase::GetFeatureInfo(wgpu::FeatureName feature) {
        return mFeaturesInfo.GetFeatureInfo(feature);
    }

    const std::vector<Ref<AdapterBase>>& InstanceBase::GetAdapters() const {
        return mAdapters;
    }

    void InstanceBase::EnsureBackendConnection(wgpu::BackendType backendType) {
        if (mBackendsConnected[backendType]) {
            return;
        }

        auto Register = [this](BackendConnection* connection, wgpu::BackendType expectedType) {
            if (connection != nullptr) {
                ASSERT(connection->GetType() == expectedType);
                ASSERT(connection->GetInstance() == this);
                mBackends.push_back(std::unique_ptr<BackendConnection>(connection));
            }
        };

        switch (backendType) {
#if defined(DAWN_ENABLE_BACKEND_NULL)
            case wgpu::BackendType::Null:
                Register(null::Connect(this), wgpu::BackendType::Null);
                break;
#endif  // defined(DAWN_ENABLE_BACKEND_NULL)

#if defined(DAWN_ENABLE_BACKEND_D3D12)
            case wgpu::BackendType::D3D12:
                Register(d3d12::Connect(this), wgpu::BackendType::D3D12);
                break;
#endif  // defined(DAWN_ENABLE_BACKEND_D3D12)

#if defined(DAWN_ENABLE_BACKEND_METAL)
            case wgpu::BackendType::Metal:
                Register(metal::Connect(this), wgpu::BackendType::Metal);
                break;
#endif  // defined(DAWN_ENABLE_BACKEND_METAL)

#if defined(DAWN_ENABLE_BACKEND_VULKAN)
            case wgpu::BackendType::Vulkan:
                Register(vulkan::Connect(this), wgpu::BackendType::Vulkan);
                break;
#endif  // defined(DAWN_ENABLE_BACKEND_VULKAN)

#if defined(DAWN_ENABLE_BACKEND_DESKTOP_GL)
            case wgpu::BackendType::OpenGL:
                Register(opengl::Connect(this, wgpu::BackendType::OpenGL),
                         wgpu::BackendType::OpenGL);
                break;
#endif  // defined(DAWN_ENABLE_BACKEND_DESKTOP_GL)

#if defined(DAWN_ENABLE_BACKEND_OPENGLES)
            case wgpu::BackendType::OpenGLES:
                Register(opengl::Connect(this, wgpu::BackendType::OpenGLES),
                         wgpu::BackendType::OpenGLES);
                break;
#endif  // defined(DAWN_ENABLE_BACKEND_OPENGLES)

            default:
                UNREACHABLE();
        }

        mBackendsConnected.set(backendType);
    }

    MaybeError InstanceBase::DiscoverAdaptersInternal(const AdapterDiscoveryOptionsBase* options) {
        wgpu::BackendType backendType = static_cast<wgpu::BackendType>(options->backendType);
        DAWN_TRY(ValidateBackendType(backendType));

        if (!GetEnabledBackends()[backendType]) {
            return DAWN_FORMAT_VALIDATION_ERROR("%s not supported.", backendType);
        }

        EnsureBackendConnection(backendType);

        bool foundBackend = false;
        for (std::unique_ptr<BackendConnection>& backend : mBackends) {
            if (backend->GetType() != backendType) {
                continue;
            }
            foundBackend = true;

            std::vector<Ref<AdapterBase>> newAdapters;
            DAWN_TRY_ASSIGN(newAdapters, backend->DiscoverAdapters(options));

            for (Ref<AdapterBase>& adapter : newAdapters) {
                ASSERT(adapter->GetBackendType() == backend->GetType());
                ASSERT(adapter->GetInstance() == this);
                mAdapters.push_back(std::move(adapter));
            }
        }

        DAWN_INVALID_IF(!foundBackend, "%s not available.", backendType);
        return {};
    }

    bool InstanceBase::ConsumedError(MaybeError maybeError) {
        if (maybeError.IsError()) {
            std::unique_ptr<ErrorData> error = maybeError.AcquireError();

            ASSERT(error != nullptr);
            dawn::ErrorLog() << error->GetFormattedMessage();
            return true;
        }
        return false;
    }

    bool InstanceBase::IsBackendValidationEnabled() const {
        return mBackendValidationLevel != BackendValidationLevel::Disabled;
    }

    void InstanceBase::SetBackendValidationLevel(BackendValidationLevel level) {
        mBackendValidationLevel = level;
    }

    BackendValidationLevel InstanceBase::GetBackendValidationLevel() const {
        return mBackendValidationLevel;
    }

    void InstanceBase::EnableBeginCaptureOnStartup(bool beginCaptureOnStartup) {
        mBeginCaptureOnStartup = beginCaptureOnStartup;
    }

    bool InstanceBase::IsBeginCaptureOnStartupEnabled() const {
        return mBeginCaptureOnStartup;
    }

    void InstanceBase::SetPlatform(dawn::platform::Platform* platform) {
        mPlatform = platform;
    }

    dawn::platform::Platform* InstanceBase::GetPlatform() {
        if (mPlatform != nullptr) {
            return mPlatform;
        }

        if (mDefaultPlatform == nullptr) {
            mDefaultPlatform = std::make_unique<dawn::platform::Platform>();
        }
        return mDefaultPlatform.get();
    }

    const std::vector<std::string>& InstanceBase::GetRuntimeSearchPaths() const {
        return mRuntimeSearchPaths;
    }

    const XlibXcbFunctions* InstanceBase::GetOrCreateXlibXcbFunctions() {
#if defined(DAWN_USE_X11)
        if (mXlibXcbFunctions == nullptr) {
            mXlibXcbFunctions = std::make_unique<XlibXcbFunctions>();
        }
        return mXlibXcbFunctions.get();
#else
        UNREACHABLE();
#endif  // defined(DAWN_USE_X11)
    }

    Surface* InstanceBase::APICreateSurface(const SurfaceDescriptor* descriptor) {
        if (ConsumedError(ValidateSurfaceDescriptor(this, descriptor))) {
            return nullptr;
        }

        return new Surface(this, descriptor);
    }

}  // namespace dawn::native
