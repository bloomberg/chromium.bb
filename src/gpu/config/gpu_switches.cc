// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/config/gpu_switches.h"

namespace switches {

// Disable GPU rasterization, i.e. rasterize on the CPU only.
// Overrides the kEnableGpuRasterization flag.
const char kDisableGpuRasterization[] = "disable-gpu-rasterization";

// Allow heuristics to determine when a layer tile should be drawn with the
// Skia GPU backend. Only valid with GPU accelerated compositing.
const char kEnableGpuRasterization[] = "enable-gpu-rasterization";

// Select a different set of GPU blacklist entries with the specificed
// test_group ID.
const char kGpuBlacklistTestGroup[] = "gpu-blacklist-test-group";

// Enable an extra set of GPU driver bug list entries with the specified
// test_group ID. Note the default test group (group 0) is still active.
const char kGpuDriverBugListTestGroup[] = "gpu-driver-bug-list-test-group";

// Passes encoded GpuPreferences to GPU process.
const char kGpuPreferences[] = "gpu-preferences";

// Ignores GPU blacklist.
const char kIgnoreGpuBlacklist[] = "ignore-gpu-blacklist";

// Allows explicitly specifying the shader disk cache size for embedded devices.
// Default value is 6MB. On Android, 2MB is default and 128KB for low-end
// devices.
const char kShaderDiskCacheSizeKB[] = "shader-disk-cache-size-kb";

// Disables the non-sandboxed GPU process for DX12 and Vulkan info collection
const char kDisableGpuProcessForDX12VulkanInfoCollection[] =
    "disable-gpu-process-for-dx12-vulkan-info-collection";

const char kEnableUnsafeWebGPU[] = "enable-unsafe-webgpu";

// Enable validation layers in Dawn backends.
const char kEnableDawnBackendValidation[] = "enable-dawn-backend-validation";

// Increases the priority (to REALTIME_AUDIO) of gpu process and compositor
// thread.
// This is only to be used for perf tests on macOS for more reliable values.
const char kUseHighGPUThreadPriorityForPerfTests[] =
    "use-gpu-high-thread-priority-for-perf-tests";

// Start the non-sandboxed GPU process for DX12 and Vulkan info collection
// immediately after the browser starts. The default is to delay for 120
// seconds.
const char kNoDelayForDX12VulkanInfoCollection[] =
    "no-delay-for-dx12-vulkan-info-collection";

// Enables measures of how long GPU Main Thread was blocked between SwapBuffers
const char kEnableGpuBlockedTime[] = "enable-gpu-blocked-time";

// Passes the active graphics vendor id from browser process to info collection
// GPU process.
const char kGpuVendorId[] = "gpu-vendor-id";

// Passes the active graphics device id from browser process to info collection
// GPU process.
const char kGpuDeviceId[] = "gpu-device-id";

// Passes the active graphics sub system id from browser process to info
// collection GPU process.
const char kGpuSubSystemId[] = "gpu-sub-system-id";

// Passes the active graphics revision info from browser process to info
// collection GPU process.
const char kGpuRevision[] = "gpu-revision";

// Passes the active graphics driver version from browser process to info
// collection GPU process.
const char kGpuDriverVersion[] = "gpu-driver-version";

}  // namespace switches
