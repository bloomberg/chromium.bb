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

#include "dawn_native/metal/BackendMTL.h"

#include "common/CoreFoundationRef.h"
#include "common/GPUInfo.h"
#include "common/NSRef.h"
#include "common/Platform.h"
#include "common/SystemUtils.h"
#include "dawn_native/Instance.h"
#include "dawn_native/MetalBackend.h"
#include "dawn_native/metal/DeviceMTL.h"

#if defined(DAWN_PLATFORM_MACOS)
#    import <IOKit/IOKitLib.h>
#    include "common/IOKitRef.h"
#endif

#include <vector>

namespace dawn_native { namespace metal {

    namespace {

        struct PCIIDs {
            uint32_t vendorId;
            uint32_t deviceId;
        };

        struct Vendor {
            const char* trademark;
            uint32_t vendorId;
        };

#if defined(DAWN_PLATFORM_MACOS)
        const Vendor kVendors[] = {{"AMD", gpu_info::kVendorID_AMD},
                                   {"Radeon", gpu_info::kVendorID_AMD},
                                   {"Intel", gpu_info::kVendorID_Intel},
                                   {"Geforce", gpu_info::kVendorID_Nvidia},
                                   {"Quadro", gpu_info::kVendorID_Nvidia}};

        // Find vendor ID from MTLDevice name.
        MaybeError GetVendorIdFromVendors(id<MTLDevice> device, PCIIDs* ids) {
            uint32_t vendorId = 0;
            const char* deviceName = [device.name UTF8String];
            for (const auto& it : kVendors) {
                if (strstr(deviceName, it.trademark) != nullptr) {
                    vendorId = it.vendorId;
                    break;
                }
            }

            if (vendorId == 0) {
                return DAWN_INTERNAL_ERROR("Failed to find vendor id with the device");
            }

            // Set vendor id with 0
            *ids = PCIIDs{vendorId, 0};
            return {};
        }

        // Extracts an integer property from a registry entry.
        uint32_t GetEntryProperty(io_registry_entry_t entry, CFStringRef name) {
            uint32_t value = 0;

            // Recursively search registry entry and its parents for property name
            // The data should release with CFRelease
            CFRef<CFDataRef> data =
                AcquireCFRef(static_cast<CFDataRef>(IORegistryEntrySearchCFProperty(
                    entry, kIOServicePlane, name, kCFAllocatorDefault,
                    kIORegistryIterateRecursively | kIORegistryIterateParents)));

            if (data == nullptr) {
                return value;
            }

            // CFDataGetBytePtr() is guaranteed to return a read-only pointer
            value = *reinterpret_cast<const uint32_t*>(CFDataGetBytePtr(data.Get()));
            return value;
        }

        // Queries the IO Registry to find the PCI device and vendor IDs of the MTLDevice.
        // The registry entry correponding to [device registryID] doesn't contain the exact PCI ids
        // because it corresponds to a driver. However its parent entry corresponds to the device
        // itself and has uint32_t "device-id" and "registry-id" keys. For example on a dual-GPU
        // MacBook Pro 2017 the IORegistry explorer shows the following tree (simplified here):
        //
        //  - PCI0@0
        //  | - AppleACPIPCI
        //  | | - IGPU@2 (type IOPCIDevice)
        //  | | | - IntelAccelerator (type IOGraphicsAccelerator2)
        //  | | - PEG0@1
        //  | | | - IOPP
        //  | | | | - GFX0@0 (type IOPCIDevice)
        //  | | | | | - AMDRadeonX4000_AMDBaffinGraphicsAccelerator (type IOGraphicsAccelerator2)
        //
        // [device registryID] is the ID for one of the IOGraphicsAccelerator2 and we can see that
        // their parent always is an IOPCIDevice that has properties for the device and vendor IDs.
        MaybeError API_AVAILABLE(macos(10.13))
            GetDeviceIORegistryPCIInfo(id<MTLDevice> device, PCIIDs* ids) {
            // Get a matching dictionary for the IOGraphicsAccelerator2
            CFRef<CFMutableDictionaryRef> matchingDict =
                AcquireCFRef(IORegistryEntryIDMatching([device registryID]));
            if (matchingDict == nullptr) {
                return DAWN_INTERNAL_ERROR("Failed to create the matching dict for the device");
            }

            // IOServiceGetMatchingService will consume the reference on the matching dictionary,
            // so we don't need to release the dictionary.
            IORef<io_registry_entry_t> acceleratorEntry = AcquireIORef(
                IOServiceGetMatchingService(kIOMasterPortDefault, matchingDict.Detach()));
            if (acceleratorEntry == IO_OBJECT_NULL) {
                return DAWN_INTERNAL_ERROR(
                    "Failed to get the IO registry entry for the accelerator");
            }

            // Get the parent entry that will be the IOPCIDevice
            IORef<io_registry_entry_t> deviceEntry;
            if (IORegistryEntryGetParentEntry(acceleratorEntry.Get(), kIOServicePlane,
                                              deviceEntry.InitializeInto()) != kIOReturnSuccess) {
                return DAWN_INTERNAL_ERROR("Failed to get the IO registry entry for the device");
            }

            ASSERT(deviceEntry != IO_OBJECT_NULL);

            uint32_t vendorId = GetEntryProperty(deviceEntry.Get(), CFSTR("vendor-id"));
            uint32_t deviceId = GetEntryProperty(deviceEntry.Get(), CFSTR("device-id"));

            *ids = PCIIDs{vendorId, deviceId};

            return {};
        }

        MaybeError GetDevicePCIInfo(id<MTLDevice> device, PCIIDs* ids) {
            // [device registryID] is introduced on macOS 10.13+, otherwise workaround to get vendor
            // id by vendor name on old macOS
            if (@available(macos 10.13, *)) {
                return GetDeviceIORegistryPCIInfo(device, ids);
            } else {
                return GetVendorIdFromVendors(device, ids);
            }
        }

        bool IsMetalSupported() {
            // Metal was first introduced in macOS 10.11
            return IsMacOSVersionAtLeast(10, 11);
        }
#elif defined(DAWN_PLATFORM_IOS)
        MaybeError GetDevicePCIInfo(id<MTLDevice> device, PCIIDs* ids) {
            DAWN_UNUSED(device);
            *ids = PCIIDs{0, 0};
            return {};
        }

        bool IsMetalSupported() {
            return true;
        }
#else
#    error "Unsupported Apple platform."
#endif

        bool IsCounterSamplingBoundarySupport(id<MTLDevice> device)
            API_AVAILABLE(macos(11.0), ios(14.0)) {
            bool isBlitBoundarySupported =
                [device supportsCounterSampling:MTLCounterSamplingPointAtBlitBoundary];
            bool isDispatchBoundarySupported =
                [device supportsCounterSampling:MTLCounterSamplingPointAtDispatchBoundary];
            bool isDrawBoundarySupported =
                [device supportsCounterSampling:MTLCounterSamplingPointAtDrawBoundary];

            return isBlitBoundarySupported && isDispatchBoundarySupported &&
                   isDrawBoundarySupported;
        }

        bool IsGPUCounterSupported(id<MTLDevice> device,
                                   MTLCommonCounterSet counterSetName,
                                   std::vector<MTLCommonCounter> counters)
            API_AVAILABLE(macos(10.15), ios(14.0)) {
            // MTLDevice’s counterSets property declares which counter sets it supports. Check
            // whether it's available on the device before requesting a counter set.
            id<MTLCounterSet> counterSet = nil;
            for (id<MTLCounterSet> set in device.counterSets) {
                if ([set.name caseInsensitiveCompare:counterSetName] == NSOrderedSame) {
                    counterSet = set;
                    break;
                }
            }

            // The counter set is not supported.
            if (counterSet == nil) {
                return false;
            }

            // A GPU might support a counter set, but only support a subset of the counters in that
            // set, check if the counter set supports all specific counters we need. Return false
            // if there is a counter unsupported.
            std::vector<NSString*> supportedCounters;
            for (id<MTLCounter> counter in counterSet.counters) {
                supportedCounters.push_back(counter.name);
            }
            for (const auto& counterName : counters) {
                if (std::find(supportedCounters.begin(), supportedCounters.end(), counterName) ==
                    supportedCounters.end()) {
                    return false;
                }
            }

            if (@available(macOS 11.0, iOS 14.0, *)) {
                // Check whether it can read GPU counters at the specified command boundary. Apple
                // family GPUs do not support sampling between different Metal commands, because
                // they defer fragment processing until after the GPU processes all the primitives
                // in the render pass.
                if (!IsCounterSamplingBoundarySupport(device)) {
                    return false;
                }
            }

            return true;
        }

    }  // anonymous namespace

    // The Metal backend's Adapter.

    class Adapter : public AdapterBase {
      public:
        Adapter(InstanceBase* instance, id<MTLDevice> device)
            : AdapterBase(instance, wgpu::BackendType::Metal), mDevice(device) {
            mPCIInfo.name = std::string([[*mDevice name] UTF8String]);

            PCIIDs ids;
            if (!instance->ConsumedError(GetDevicePCIInfo(device, &ids))) {
                mPCIInfo.vendorId = ids.vendorId;
                mPCIInfo.deviceId = ids.deviceId;
            }

#if defined(DAWN_PLATFORM_IOS)
            mAdapterType = wgpu::AdapterType::IntegratedGPU;
            const char* systemName = "iOS ";
#elif defined(DAWN_PLATFORM_MACOS)
            if ([device isLowPower]) {
                mAdapterType = wgpu::AdapterType::IntegratedGPU;
            } else {
                mAdapterType = wgpu::AdapterType::DiscreteGPU;
            }
            const char* systemName = "macOS ";
#else
#    error "Unsupported Apple platform."
#endif

            NSString* osVersion = [[NSProcessInfo processInfo] operatingSystemVersionString];
            mDriverDescription =
                "Metal driver on " + std::string(systemName) + [osVersion UTF8String];

            InitializeSupportedFeatures();
        }

        // AdapterBase Implementation
        bool SupportsExternalImages() const override {
            // Via dawn_native::metal::WrapIOSurface
            return true;
        }

      private:
        ResultOrError<DeviceBase*> CreateDeviceImpl(const DeviceDescriptor* descriptor) override {
            return Device::Create(this, mDevice, descriptor);
        }

        void InitializeSupportedFeatures() {
#if defined(DAWN_PLATFORM_MACOS)
            if ([*mDevice supportsFeatureSet:MTLFeatureSet_macOS_GPUFamily1_v1]) {
                mSupportedFeatures.EnableFeature(Feature::TextureCompressionBC);
            }
#endif

            if (@available(macOS 10.15, iOS 14.0, *)) {
                if (IsGPUCounterSupported(
                        *mDevice, MTLCommonCounterSetStatistic,
                        {MTLCommonCounterVertexInvocations, MTLCommonCounterClipperInvocations,
                         MTLCommonCounterClipperPrimitivesOut, MTLCommonCounterFragmentInvocations,
                         MTLCommonCounterComputeKernelInvocations})) {
                    mSupportedFeatures.EnableFeature(Feature::PipelineStatisticsQuery);
                }

                if (IsGPUCounterSupported(*mDevice, MTLCommonCounterSetTimestamp,
                                          {MTLCommonCounterTimestamp})) {
                    // Disable timestamp query on macOS 10.15 on AMD GPU because WriteTimestamp
                    // fails to call without any copy commands on MTLBlitCommandEncoder. This issue
                    // has been fixed on macOS 11.0. See crbug.com/dawn/545
                    BOOL enableTimestampQuery = !gpu_info::IsAMD(GetPCIInfo().vendorId);
#if defined(DAWN_PLATFORM_MACOS)
                    // TODO(crbug.com/dawn/940): Disable timestamp query on macOS 11.0+. Need to
                    // figure out what to do with dstOffset alignment on that system.
                    enableTimestampQuery &= !IsMacOSVersionAtLeast(11);
#endif
                    if (enableTimestampQuery) {
                        mSupportedFeatures.EnableFeature(Feature::TimestampQuery);
                    }
                }
            }

            if (@available(macOS 10.11, iOS 11.0, *)) {
                mSupportedFeatures.EnableFeature(Feature::DepthClamping);
            }
        }

        NSPRef<id<MTLDevice>> mDevice;
    };

    // Implementation of the Metal backend's BackendConnection

    Backend::Backend(InstanceBase* instance)
        : BackendConnection(instance, wgpu::BackendType::Metal) {
        if (GetInstance()->IsBackendValidationEnabled()) {
            setenv("METAL_DEVICE_WRAPPER_TYPE", "1", 1);
        }
    }

    std::vector<std::unique_ptr<AdapterBase>> Backend::DiscoverDefaultAdapters() {
        std::vector<std::unique_ptr<AdapterBase>> adapters;
        BOOL supportedVersion = NO;
#if defined(DAWN_PLATFORM_MACOS)
        if (@available(macOS 10.11, *)) {
            supportedVersion = YES;

            NSRef<NSArray<id<MTLDevice>>> devices = AcquireNSRef(MTLCopyAllDevices());

            for (id<MTLDevice> device in devices.Get()) {
                adapters.push_back(std::make_unique<Adapter>(GetInstance(), device));
            }
        }
#endif

#if defined(DAWN_PLATFORM_IOS)
        if (@available(iOS 8.0, *)) {
            supportedVersion = YES;
            // iOS only has a single device so MTLCopyAllDevices doesn't exist there.
            adapters.push_back(
                std::make_unique<Adapter>(GetInstance(), MTLCreateSystemDefaultDevice()));
        }
#endif
        if (!supportedVersion) {
            UNREACHABLE();
        }
        return adapters;
    }

    BackendConnection* Connect(InstanceBase* instance) {
        if (!IsMetalSupported()) {
            return nullptr;
        }
        return new Backend(instance);
    }

}}  // namespace dawn_native::metal
