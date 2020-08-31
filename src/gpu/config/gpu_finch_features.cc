// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "gpu/config/gpu_finch_features.h"

#if defined(OS_ANDROID)
#include "base/android/build_info.h"
#include "base/metrics/field_trial_params.h"
#include "base/strings/string_split.h"
#include "ui/gl/android/android_surface_control_compat.h"
#endif

namespace features {

#if defined(OS_ANDROID)
// Used only by webview to disable SurfaceControl.
const base::Feature kDisableSurfaceControlForWebview{
    "DisableSurfaceControlForWebview", base::FEATURE_DISABLED_BY_DEFAULT};

// Used to limit GL version to 2.0 for skia raster on Android.
const base::Feature kUseGles2ForOopR{"UseGles2ForOopR",
                                     base::FEATURE_ENABLED_BY_DEFAULT};
#endif

// Enable GPU Rasterization by default. This can still be overridden by
// --force-gpu-rasterization or --disable-gpu-rasterization.
#if defined(OS_MACOSX) || defined(OS_WIN) || defined(OS_CHROMEOS) || \
    defined(OS_ANDROID) || defined(OS_FUCHSIA)
// DefaultEnableGpuRasterization has launched on Mac, Windows, ChromeOS, and
// Android.
const base::Feature kDefaultEnableGpuRasterization{
    "DefaultEnableGpuRasterization", base::FEATURE_ENABLED_BY_DEFAULT};
#else
const base::Feature kDefaultEnableGpuRasterization{
    "DefaultEnableGpuRasterization", base::FEATURE_DISABLED_BY_DEFAULT};
#endif

// Enable out of process rasterization by default.  This can still be overridden
// by --enable-oop-rasterization or --disable-oop-rasterization.
#if defined(OS_ANDROID) || defined(OS_CHROMEOS) || defined(OS_MACOSX)
const base::Feature kDefaultEnableOopRasterization{
    "DefaultEnableOopRasterization", base::FEATURE_ENABLED_BY_DEFAULT};
#else
const base::Feature kDefaultEnableOopRasterization{
    "DefaultEnableOopRasterization", base::FEATURE_DISABLED_BY_DEFAULT};
#endif

// Allow putting a video swapchain underneath the main swapchain, so overlays
// can be used even if there are controls on top of the video. It can be
// enabled only when overlay is supported.
const base::Feature kDirectCompositionUnderlays{
    "DirectCompositionUnderlays", base::FEATURE_ENABLED_BY_DEFAULT};

#if defined(OS_WIN)
// Use a high priority for GPU process on Windows.
const base::Feature kGpuProcessHighPriorityWin{
    "GpuProcessHighPriorityWin", base::FEATURE_ENABLED_BY_DEFAULT};
#endif

// Use ThreadPriority::DISPLAY for GPU main, viz compositor and IO threads.
#if defined(OS_ANDROID) || defined(OS_CHROMEOS) || defined(OS_WIN)
const base::Feature kGpuUseDisplayThreadPriority{
    "GpuUseDisplayThreadPriority", base::FEATURE_ENABLED_BY_DEFAULT};
#else
const base::Feature kGpuUseDisplayThreadPriority{
    "GpuUseDisplayThreadPriority", base::FEATURE_DISABLED_BY_DEFAULT};
#endif

// Gpu watchdog V2 to simplify the logic and reduce GPU hangs
const base::Feature kGpuWatchdogV2{"GpuWatchdogV2",
                                   base::FEATURE_ENABLED_BY_DEFAULT};

// Use a different set of watchdog timeouts on V1
const base::Feature kGpuWatchdogV1NewTimeout{"GpuWatchdogV1NewTimeout",
                                             base::FEATURE_ENABLED_BY_DEFAULT};

// Use a different set of watchdog timeouts on V2
const base::Feature kGpuWatchdogV2NewTimeout{"GpuWatchdogV2NewTimeout",
                                             base::FEATURE_DISABLED_BY_DEFAULT};

#if defined(OS_MACOSX)
// Enable use of Metal for OOP rasterization.
const base::Feature kMetal{"Metal", base::FEATURE_DISABLED_BY_DEFAULT};
#endif

// Turns on skia deferred display list for out of process raster.
const base::Feature kOopRasterizationDDL{"OopRasterizationDDL",
                                         base::FEATURE_DISABLED_BY_DEFAULT};

// Causes us to use the SharedImageManager, removing support for the old
// mailbox system. Any consumers of the GPU process using the old mailbox
// system will experience undefined results.
const base::Feature kSharedImageManager{"SharedImageManager",
                                        base::FEATURE_DISABLED_BY_DEFAULT};

// Controls the decode acceleration of JPEG images (as opposed to camera
// captures) in Chrome OS using the VA-API.
// TODO(andrescj): remove or enable by default in Chrome OS once
// https://crbug.com/868400 is resolved.
const base::Feature kVaapiJpegImageDecodeAcceleration{
    "VaapiJpegImageDecodeAcceleration", base::FEATURE_DISABLED_BY_DEFAULT};

// Controls the decode acceleration of WebP images in Chrome OS using the
// VA-API.
// TODO(gildekel): remove or enable by default in Chrome OS once
// https://crbug.com/877694 is resolved.
const base::Feature kVaapiWebPImageDecodeAcceleration{
    "VaapiWebPImageDecodeAcceleration", base::FEATURE_DISABLED_BY_DEFAULT};

// Enable Vulkan graphics backend for compositing and rasterization. Defaults to
// native implementation if --use-vulkan flag is not used. Otherwise
// --use-vulkan will be followed.
const base::Feature kVulkan{"Vulkan", base::FEATURE_DISABLED_BY_DEFAULT};

// Enable SkiaRenderer Dawn graphics backend. On Windows this will use D3D12,
// and on Linux this will use Vulkan.
const base::Feature kSkiaDawn{"SkiaDawn", base::FEATURE_DISABLED_BY_DEFAULT};

// Used to enable shared image mailbox and disable legacy texture mailbox on
// webview.
const base::Feature kEnableSharedImageForWebview{
    "EnableSharedImageForWebview", base::FEATURE_DISABLED_BY_DEFAULT};

#if defined(OS_ANDROID)
bool IsAndroidSurfaceControlEnabled() {
  if (base::FeatureList::IsEnabled(kDisableSurfaceControlForWebview))
    return false;

  return gl::SurfaceControl::IsSupported();
}
#endif

}  // namespace features
