// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_CONFIG_GPU_SWITCHES_H_
#define GPU_CONFIG_GPU_SWITCHES_H_

#include "gpu/gpu_export.h"

namespace switches {

GPU_EXPORT extern const char kDisableGpuRasterization[];
GPU_EXPORT extern const char kDisableMipmapGeneration[];
GPU_EXPORT extern const char kEnableGpuRasterization[];
GPU_EXPORT extern const char kGpuBlocklistTestGroup[];
GPU_EXPORT extern const char kGpuDriverBugListTestGroup[];
GPU_EXPORT extern const char kGpuPreferences[];
GPU_EXPORT extern const char kIgnoreGpuBlocklist[];
GPU_EXPORT extern const char kShaderDiskCacheSizeKB[];
GPU_EXPORT extern const char kDisableGpuProcessForDX12InfoCollection[];
GPU_EXPORT extern const char kEnableUnsafeWebGPU[];
GPU_EXPORT extern const char kEnableDawnBackendValidation[];
GPU_EXPORT extern const char kForceWebGPUCompat[];
GPU_EXPORT extern const char kEnableDawnFeatures[];
GPU_EXPORT extern const char kDisableDawnFeatures[];
GPU_EXPORT extern const char kUseHighGPUThreadPriorityForPerfTests[];
GPU_EXPORT extern const char kNoDelayForDX12VulkanInfoCollection[];
GPU_EXPORT extern const char kEnableGpuBlockedTime[];
GPU_EXPORT extern const char kGpuVendorId[];
GPU_EXPORT extern const char kGpuDeviceId[];
GPU_EXPORT extern const char kGpuSubSystemId[];
GPU_EXPORT extern const char kGpuRevision[];
GPU_EXPORT extern const char kGpuDriverVersion[];
GPU_EXPORT extern const char kWebViewDrawFunctorUsesVulkan[];
GPU_EXPORT extern const char kEnableVulkanProtectedMemory[];
GPU_EXPORT extern const char kDisableVulkanFallbackToGLForTesting[];
GPU_EXPORT extern const char kVulkanHeapMemoryLimitMb[];
GPU_EXPORT extern const char kVulkanSyncCpuMemoryLimitMb[];
GPU_EXPORT extern const char kForceBrowserCrashOnGpuCrash[];
GPU_EXPORT extern const char kGpuWatchdogTimeoutSeconds[];

}  // namespace switches

#endif  // GPU_CONFIG_GPU_SWITCHES_H_
