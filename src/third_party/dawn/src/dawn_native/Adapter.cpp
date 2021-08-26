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

#include "dawn_native/Adapter.h"

#include "dawn_native/Instance.h"

namespace dawn_native {

    AdapterBase::AdapterBase(InstanceBase* instance, wgpu::BackendType backend)
        : mInstance(instance), mBackend(backend) {
        mSupportedExtensions.EnableExtension(Extension::DawnInternalUsages);
    }

    wgpu::BackendType AdapterBase::GetBackendType() const {
        return mBackend;
    }

    wgpu::AdapterType AdapterBase::GetAdapterType() const {
        return mAdapterType;
    }

    const std::string& AdapterBase::GetDriverDescription() const {
        return mDriverDescription;
    }

    const PCIInfo& AdapterBase::GetPCIInfo() const {
        return mPCIInfo;
    }

    InstanceBase* AdapterBase::GetInstance() const {
        return mInstance;
    }

    ExtensionsSet AdapterBase::GetSupportedExtensions() const {
        return mSupportedExtensions;
    }

    bool AdapterBase::SupportsAllRequestedExtensions(
        const std::vector<const char*>& requestedExtensions) const {
        for (const char* extensionStr : requestedExtensions) {
            Extension extensionEnum = mInstance->ExtensionNameToEnum(extensionStr);
            if (extensionEnum == Extension::InvalidEnum) {
                return false;
            }
            if (!mSupportedExtensions.IsEnabled(extensionEnum)) {
                return false;
            }
        }
        return true;
    }

    WGPUDeviceProperties AdapterBase::GetAdapterProperties() const {
        WGPUDeviceProperties adapterProperties = {};

        mSupportedExtensions.InitializeDeviceProperties(&adapterProperties);
        return adapterProperties;
    }

    DeviceBase* AdapterBase::CreateDevice(const DeviceDescriptor* descriptor) {
        DeviceBase* result = nullptr;

        if (mInstance->ConsumedError(CreateDeviceInternal(&result, descriptor))) {
            return nullptr;
        }

        return result;
    }

    MaybeError AdapterBase::CreateDeviceInternal(DeviceBase** result,
                                                 const DeviceDescriptor* descriptor) {
        if (descriptor != nullptr) {
            if (!SupportsAllRequestedExtensions(descriptor->requiredExtensions)) {
                return DAWN_VALIDATION_ERROR("One or more requested extensions are not supported");
            }
        }

        DAWN_TRY_ASSIGN(*result, CreateDeviceImpl(descriptor));
        return {};
    }

    void AdapterBase::ResetInternalDeviceForTesting() {
        mInstance->ConsumedError(ResetInternalDeviceForTestingImpl());
    }

    MaybeError AdapterBase::ResetInternalDeviceForTestingImpl() {
        return DAWN_INTERNAL_ERROR(
            "ResetInternalDeviceForTesting should only be used with the D3D12 backend.");
    }

}  // namespace dawn_native
