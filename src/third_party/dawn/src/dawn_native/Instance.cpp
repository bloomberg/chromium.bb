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

#include "dawn_native/Instance.h"

#include "common/Assert.h"
#include "common/Log.h"
#include "dawn_native/ErrorData.h"
#include "dawn_native/Surface.h"
#include "dawn_platform/DawnPlatform.h"

#if defined(DAWN_USE_X11)
#    include "dawn_native/XlibXcbFunctions.h"
#endif  // defined(DAWN_USE_X11)

namespace dawn_native {

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
        BackendConnection* Connect(InstanceBase* instance, bool useSwiftshader);
    }
#endif  // defined(DAWN_ENABLE_BACKEND_VULKAN)

    // InstanceBase

    // static
    InstanceBase* InstanceBase::Create(const InstanceDescriptor* descriptor) {
        Ref<InstanceBase> instance = AcquireRef(new InstanceBase);
        if (!instance->Initialize(descriptor)) {
            return nullptr;
        }
        return instance.Detach();
    }

    // TODO(crbug.com/dawn/832): make the platform an initialization parameter of the instance.
    bool InstanceBase::Initialize(const InstanceDescriptor*) {
        return true;
    }

    void InstanceBase::DiscoverDefaultAdapters() {
        EnsureBackendConnections();

        if (mDiscoveredDefaultAdapters) {
            return;
        }

        // Query and merge all default adapters for all backends
        for (std::unique_ptr<BackendConnection>& backend : mBackends) {
            std::vector<std::unique_ptr<AdapterBase>> backendAdapters =
                backend->DiscoverDefaultAdapters();

            for (std::unique_ptr<AdapterBase>& adapter : backendAdapters) {
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

    const FeatureInfo* InstanceBase::GetFeatureInfo(const char* featureName) {
        return mFeaturesInfo.GetFeatureInfo(featureName);
    }

    Feature InstanceBase::FeatureNameToEnum(const char* featureName) {
        return mFeaturesInfo.FeatureNameToEnum(featureName);
    }

    FeaturesSet InstanceBase::FeatureNamesToFeaturesSet(
        const std::vector<const char*>& requiredFeatures) {
        return mFeaturesInfo.FeatureNamesToFeaturesSet(requiredFeatures);
    }

    const std::vector<std::unique_ptr<AdapterBase>>& InstanceBase::GetAdapters() const {
        return mAdapters;
    }

    void InstanceBase::EnsureBackendConnections() {
        if (mBackendsConnected) {
            return;
        }

        auto Register = [this](BackendConnection* connection, wgpu::BackendType expectedType) {
            if (connection != nullptr) {
                ASSERT(connection->GetType() == expectedType);
                ASSERT(connection->GetInstance() == this);
                mBackends.push_back(std::unique_ptr<BackendConnection>(connection));
            }
        };

#if defined(DAWN_ENABLE_BACKEND_D3D12)
        Register(d3d12::Connect(this), wgpu::BackendType::D3D12);
#endif  // defined(DAWN_ENABLE_BACKEND_D3D12)
#if defined(DAWN_ENABLE_BACKEND_METAL)
        Register(metal::Connect(this), wgpu::BackendType::Metal);
#endif  // defined(DAWN_ENABLE_BACKEND_METAL)
#if defined(DAWN_ENABLE_BACKEND_VULKAN)
        // TODO(https://github.com/KhronosGroup/Vulkan-Loader/issues/287):
        // When we can load SwiftShader in parallel with the system driver, we should create the
        // backend only once and expose SwiftShader as an additional adapter. For now, we create two
        // VkInstances, one from SwiftShader, and one from the system. Note: If the Vulkan driver
        // *is* SwiftShader, then this would load SwiftShader twice.
        Register(vulkan::Connect(this, false), wgpu::BackendType::Vulkan);
#    if defined(DAWN_ENABLE_SWIFTSHADER)
        Register(vulkan::Connect(this, true), wgpu::BackendType::Vulkan);
#    endif  // defined(DAWN_ENABLE_SWIFTSHADER)
#endif      // defined(DAWN_ENABLE_BACKEND_VULKAN)
#if defined(DAWN_ENABLE_BACKEND_DESKTOP_GL)
        Register(opengl::Connect(this, wgpu::BackendType::OpenGL), wgpu::BackendType::OpenGL);
#endif  // defined(DAWN_ENABLE_BACKEND_DESKTOP_GL)
#if defined(DAWN_ENABLE_BACKEND_OPENGLES)
        Register(opengl::Connect(this, wgpu::BackendType::OpenGLES), wgpu::BackendType::OpenGLES);
#endif  // defined(DAWN_ENABLE_BACKEND_OPENGLES)
#if defined(DAWN_ENABLE_BACKEND_NULL)
        Register(null::Connect(this), wgpu::BackendType::Null);
#endif  // defined(DAWN_ENABLE_BACKEND_NULL)

        mBackendsConnected = true;
    }

    MaybeError InstanceBase::DiscoverAdaptersInternal(const AdapterDiscoveryOptionsBase* options) {
        EnsureBackendConnections();

        bool foundBackend = false;
        for (std::unique_ptr<BackendConnection>& backend : mBackends) {
            if (backend->GetType() != static_cast<wgpu::BackendType>(options->backendType)) {
                continue;
            }
            foundBackend = true;

            std::vector<std::unique_ptr<AdapterBase>> newAdapters;
            DAWN_TRY_ASSIGN(newAdapters, backend->DiscoverAdapters(options));

            for (std::unique_ptr<AdapterBase>& adapter : newAdapters) {
                ASSERT(adapter->GetBackendType() == backend->GetType());
                ASSERT(adapter->GetInstance() == this);
                mAdapters.push_back(std::move(adapter));
            }
        }

        if (!foundBackend) {
            return DAWN_VALIDATION_ERROR("Backend isn't present.");
        }
        return {};
    }

    bool InstanceBase::ConsumedError(MaybeError maybeError) {
        if (maybeError.IsError()) {
            std::unique_ptr<ErrorData> error = maybeError.AcquireError();

            ASSERT(error != nullptr);
            dawn::InfoLog() << error->GetFormattedMessage();

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

    void InstanceBase::SetPlatform(dawn_platform::Platform* platform) {
        mPlatform = platform;
    }

    dawn_platform::Platform* InstanceBase::GetPlatform() {
        if (mPlatform != nullptr) {
            return mPlatform;
        }

        if (mDefaultPlatform == nullptr) {
            mDefaultPlatform = std::make_unique<dawn_platform::Platform>();
        }
        return mDefaultPlatform.get();
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

}  // namespace dawn_native
