// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/config/gpu_util.h"

#if defined(OS_WIN)
#include <windows.h>
// Must be included after windows.h.
#include <psapi.h>
#endif  // OS_WIN

#include <memory>
#include <set>
#include <string>
#include <thread>
#include <vector>

#include "base/base_paths.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/stringprintf.h"
#include "base/system/sys_info.h"
#include "build/build_config.h"
#include "gpu/config/device_perf_info.h"
#include "gpu/config/gpu_blocklist.h"
#include "gpu/config/gpu_crash_keys.h"
#include "gpu/config/gpu_driver_bug_list.h"
#include "gpu/config/gpu_driver_bug_workaround_type.h"
#include "gpu/config/gpu_feature_type.h"
#include "gpu/config/gpu_finch_features.h"
#include "gpu/config/gpu_info.h"
#include "gpu/config/gpu_info_collector.h"
#include "gpu/config/gpu_preferences.h"
#include "gpu/config/gpu_switches.h"
#include "gpu/vulkan/buildflags.h"
#include "third_party/vulkan_headers/include/vulkan/vulkan.h"
#include "ui/gfx/extension_set.h"
#include "ui/gl/buildflags.h"
#include "ui/gl/gl_switches.h"

#if defined(OS_ANDROID)
#include "base/no_destructor.h"
#include "base/synchronization/lock.h"
#include "ui/gl/android/android_surface_control_compat.h"
#include "ui/gl/gl_surface_egl.h"
#include "ui/gl/init/gl_factory.h"
#endif  // OS_ANDROID

namespace gpu {

namespace {

#if defined(OS_WIN)
// These values are persistent to logs. Entries should not be renumbered and
// numeric values should never be reused.
// This should match enum D3D11FeatureLevel in
//  \tools\metrics\histograms\enums.xml
enum class D3D11FeatureLevel {
  kUnknown = 0,
  k9_1 = 4,
  k9_2 = 5,
  k9_3 = 6,
  k10_0 = 7,
  k10_1 = 8,
  k11_0 = 9,
  k11_1 = 10,
  k12_0 = 11,
  k12_1 = 12,
  kMaxValue = k12_1,
};

inline D3D11FeatureLevel ConvertToHistogramD3D11FeatureLevel(
    D3D_FEATURE_LEVEL d3d11_feature_level) {
  switch (d3d11_feature_level) {
    case D3D_FEATURE_LEVEL_1_0_CORE:
      return D3D11FeatureLevel::kUnknown;
    case D3D_FEATURE_LEVEL_9_1:
      return D3D11FeatureLevel::k9_1;
    case D3D_FEATURE_LEVEL_9_2:
      return D3D11FeatureLevel::k9_2;
    case D3D_FEATURE_LEVEL_9_3:
      return D3D11FeatureLevel::k9_3;
    case D3D_FEATURE_LEVEL_10_0:
      return D3D11FeatureLevel::k10_0;
    case D3D_FEATURE_LEVEL_10_1:
      return D3D11FeatureLevel::k10_1;
    case D3D_FEATURE_LEVEL_11_0:
      return D3D11FeatureLevel::k11_0;
    case D3D_FEATURE_LEVEL_11_1:
      return D3D11FeatureLevel::k11_1;
    case D3D_FEATURE_LEVEL_12_0:
      return D3D11FeatureLevel::k12_0;
    case D3D_FEATURE_LEVEL_12_1:
      return D3D11FeatureLevel::k12_1;
    default:
      NOTREACHED();
      return D3D11FeatureLevel::kUnknown;
  }
}
#endif  // OS_WIN

GpuFeatureStatus GetAndroidSurfaceControlFeatureStatus(
    const std::set<int>& blacklisted_features,
    const GpuPreferences& gpu_preferences) {
#if !defined(OS_ANDROID)
  return kGpuFeatureStatusDisabled;
#else
  if (!gpu_preferences.enable_android_surface_control)
    return kGpuFeatureStatusDisabled;

  // SurfaceControl as used by Chrome requires using GpuFence for
  // synchronization, this is based on Android native fence sync
  // support. If that is unavailable, i.e. on emulator or SwiftShader,
  // don't claim SurfaceControl support.
  if (!gl::GLSurfaceEGL::IsAndroidNativeFenceSyncSupported())
    return kGpuFeatureStatusDisabled;

  DCHECK(gl::SurfaceControl::IsSupported());
  return kGpuFeatureStatusEnabled;
#endif
}

GpuFeatureStatus GetMetalFeatureStatus(
    const std::set<int>& blacklisted_features,
    const GpuPreferences& gpu_preferences) {
#if defined(OS_MACOSX)
  if (blacklisted_features.count(GPU_FEATURE_TYPE_METAL))
    return kGpuFeatureStatusBlacklisted;

  if (!gpu_preferences.enable_metal)
    return kGpuFeatureStatusDisabled;

  return kGpuFeatureStatusEnabled;
#else
  return kGpuFeatureStatusDisabled;
#endif
}

GpuFeatureStatus GetVulkanFeatureStatus(
    const std::set<int>& blacklisted_features,
    const GpuPreferences& gpu_preferences) {
#if BUILDFLAG(ENABLE_VULKAN)
  // Only blacklist native vulkan.
  if (gpu_preferences.use_vulkan == VulkanImplementationName::kNative &&
      blacklisted_features.count(GPU_FEATURE_TYPE_VULKAN))
    return kGpuFeatureStatusBlacklisted;

  if (gpu_preferences.use_vulkan == VulkanImplementationName::kNone)
    return kGpuFeatureStatusDisabled;

  return kGpuFeatureStatusEnabled;
#else
  return kGpuFeatureStatusDisabled;
#endif
}

GpuFeatureStatus GetGpuRasterizationFeatureStatus(
    const std::set<int>& blacklisted_features,
    const base::CommandLine& command_line) {
  if (command_line.HasSwitch(switches::kDisableGpuRasterization))
    return kGpuFeatureStatusDisabled;
  else if (command_line.HasSwitch(switches::kEnableGpuRasterization))
    return kGpuFeatureStatusEnabled;

  if (blacklisted_features.count(GPU_FEATURE_TYPE_GPU_RASTERIZATION))
    return kGpuFeatureStatusBlacklisted;

  // Gpu Rasterization on platforms that are not fully enabled is controlled by
  // a finch experiment.
  if (!base::FeatureList::IsEnabled(features::kDefaultEnableGpuRasterization))
    return kGpuFeatureStatusDisabled;

  return kGpuFeatureStatusEnabled;
}

GpuFeatureStatus GetOopRasterizationFeatureStatus(
    const std::set<int>& blacklisted_features,
    const base::CommandLine& command_line,
    const GpuPreferences& gpu_preferences,
    const GPUInfo& gpu_info) {
#if defined(OS_WIN)
  // On Windows, using the validating decoder causes a lot of errors.  This
  // could be fixed independently, but validating decoder is going away.
  // See: http://crbug.com/949773.
  if (!gpu_info.passthrough_cmd_decoder)
    return kGpuFeatureStatusDisabled;
#endif
  // OOP rasterization requires GPU rasterization, so if blacklisted or
  // disabled, report the same.
  auto status =
      GetGpuRasterizationFeatureStatus(blacklisted_features, command_line);
  if (status != kGpuFeatureStatusEnabled)
    return status;

  // If we can't create a GrContext for whatever reason, don't enable oop
  // rasterization.
  if (!gpu_info.oop_rasterization_supported)
    return kGpuFeatureStatusDisabled;

  if (gpu_preferences.disable_oop_rasterization)
    return kGpuFeatureStatusDisabled;
  else if (gpu_preferences.enable_oop_rasterization)
    return kGpuFeatureStatusEnabled;

  // TODO(enne): Eventually oop rasterization will replace gpu rasterization,
  // and so we will need to address the underlying bugs or turn of GPU
  // rasterization for these cases.
  if (blacklisted_features.count(GPU_FEATURE_TYPE_OOP_RASTERIZATION))
    return kGpuFeatureStatusBlacklisted;

  // OOP Rasterization on platforms that are not fully enabled is controlled by
  // a finch experiment.
  if (!base::FeatureList::IsEnabled(features::kDefaultEnableOopRasterization))
    return kGpuFeatureStatusDisabled;

  return kGpuFeatureStatusEnabled;
}

GpuFeatureStatus GetWebGLFeatureStatus(
    const std::set<int>& blacklisted_features,
    bool use_swift_shader) {
  if (use_swift_shader)
    return kGpuFeatureStatusEnabled;
  if (blacklisted_features.count(GPU_FEATURE_TYPE_ACCELERATED_WEBGL))
    return kGpuFeatureStatusBlacklisted;
  return kGpuFeatureStatusEnabled;
}

GpuFeatureStatus GetWebGL2FeatureStatus(
    const std::set<int>& blacklisted_features,
    bool use_swift_shader) {
  if (use_swift_shader)
    return kGpuFeatureStatusEnabled;
  if (blacklisted_features.count(GPU_FEATURE_TYPE_ACCELERATED_WEBGL2))
    return kGpuFeatureStatusBlacklisted;
  return kGpuFeatureStatusEnabled;
}

GpuFeatureStatus Get2DCanvasFeatureStatus(
    const std::set<int>& blacklisted_features,
    bool use_swift_shader) {
  if (use_swift_shader) {
    // This is for testing only. Chrome should exercise the GPU accelerated
    // path on top of SwiftShader driver.
    return kGpuFeatureStatusEnabled;
  }
  if (blacklisted_features.count(GPU_FEATURE_TYPE_ACCELERATED_2D_CANVAS))
    return kGpuFeatureStatusSoftware;
  return kGpuFeatureStatusEnabled;
}

GpuFeatureStatus GetFlash3DFeatureStatus(
    const std::set<int>& blacklisted_features,
    bool use_swift_shader) {
  if (use_swift_shader) {
    // This is for testing only. Chrome should exercise the GPU accelerated
    // path on top of SwiftShader driver.
    return kGpuFeatureStatusEnabled;
  }
  if (blacklisted_features.count(GPU_FEATURE_TYPE_FLASH3D))
    return kGpuFeatureStatusBlacklisted;
  return kGpuFeatureStatusEnabled;
}

GpuFeatureStatus GetFlashStage3DFeatureStatus(
    const std::set<int>& blacklisted_features,
    bool use_swift_shader) {
  if (use_swift_shader) {
    // This is for testing only. Chrome should exercise the GPU accelerated
    // path on top of SwiftShader driver.
    return kGpuFeatureStatusEnabled;
  }
  if (blacklisted_features.count(GPU_FEATURE_TYPE_FLASH_STAGE3D))
    return kGpuFeatureStatusBlacklisted;
  return kGpuFeatureStatusEnabled;
}

GpuFeatureStatus GetFlashStage3DBaselineFeatureStatus(
    const std::set<int>& blacklisted_features,
    bool use_swift_shader) {
  if (use_swift_shader) {
    // This is for testing only. Chrome should exercise the GPU accelerated
    // path on top of SwiftShader driver.
    return kGpuFeatureStatusEnabled;
  }
  if (blacklisted_features.count(GPU_FEATURE_TYPE_FLASH_STAGE3D) ||
      blacklisted_features.count(GPU_FEATURE_TYPE_FLASH_STAGE3D_BASELINE))
    return kGpuFeatureStatusBlacklisted;
  return kGpuFeatureStatusEnabled;
}

GpuFeatureStatus GetAcceleratedVideoDecodeFeatureStatus(
    const std::set<int>& blacklisted_features,
    bool use_swift_shader) {
  if (use_swift_shader)
    return kGpuFeatureStatusDisabled;
  if (blacklisted_features.count(GPU_FEATURE_TYPE_ACCELERATED_VIDEO_DECODE))
    return kGpuFeatureStatusBlacklisted;
  return kGpuFeatureStatusEnabled;
}

GpuFeatureStatus GetGLFeatureStatus(const std::set<int>& blacklisted_features,
                                    bool use_swift_shader) {
  if (use_swift_shader) {
    // This is for testing only. Chrome should exercise the GPU accelerated
    // path on top of SwiftShader driver.
    return kGpuFeatureStatusEnabled;
  }
  if (blacklisted_features.count(GPU_FEATURE_TYPE_ACCELERATED_GL))
    return kGpuFeatureStatusBlacklisted;
  return kGpuFeatureStatusEnabled;
}

GpuFeatureStatus GetProtectedVideoDecodeFeatureStatus(
    const std::set<int>& blacklisted_features,
    const GPUInfo& gpu_info,
    bool use_swift_shader) {
  if (use_swift_shader)
    return kGpuFeatureStatusDisabled;
  if (blacklisted_features.count(GPU_FEATURE_TYPE_PROTECTED_VIDEO_DECODE))
    return kGpuFeatureStatusBlacklisted;
  return kGpuFeatureStatusEnabled;
}

void AppendWorkaroundsToCommandLine(const GpuFeatureInfo& gpu_feature_info,
                                    base::CommandLine* command_line) {
  if (gpu_feature_info.IsWorkaroundEnabled(DISABLE_D3D11)) {
    command_line->AppendSwitch(switches::kDisableD3D11);
  }
  if (gpu_feature_info.IsWorkaroundEnabled(DISABLE_ES3_GL_CONTEXT)) {
    command_line->AppendSwitch(switches::kDisableES3GLContext);
  }
  if (gpu_feature_info.IsWorkaroundEnabled(
          DISABLE_ES3_GL_CONTEXT_FOR_TESTING)) {
    command_line->AppendSwitch(switches::kDisableES3GLContextForTesting);
  }
#if defined(OS_WIN)
  if (gpu_feature_info.IsWorkaroundEnabled(DISABLE_DIRECT_COMPOSITION)) {
    command_line->AppendSwitch(switches::kDisableDirectComposition);
  }
  if (gpu_feature_info.IsWorkaroundEnabled(
          DISABLE_DIRECT_COMPOSITION_VIDEO_OVERLAYS)) {
    command_line->AppendSwitch(
        switches::kDisableDirectCompositionVideoOverlays);
  }
#endif
}

// Adjust gpu feature status based on enabled gpu driver bug workarounds.
void AdjustGpuFeatureStatusToWorkarounds(GpuFeatureInfo* gpu_feature_info) {
  if (gpu_feature_info->IsWorkaroundEnabled(DISABLE_D3D11) ||
      gpu_feature_info->IsWorkaroundEnabled(DISABLE_ES3_GL_CONTEXT)) {
    gpu_feature_info->status_values[GPU_FEATURE_TYPE_ACCELERATED_WEBGL2] =
        kGpuFeatureStatusBlacklisted;
  }
}

// Estimates roughly user total disk space by counting in the drives where
// the exe is, where the temporary space is, where the user home is.
// If total space and free space are of the same size, they are considered
// the same drive. There could be corner cases this estimation is far from
// the actual total disk space, but for histogram purpose, limited numbers
// of outliers do not matter.
uint32_t EstimateAmountOfTotalDiskSpaceMB() {
  const base::BasePathKey kPathKeys[] = {base::DIR_EXE, base::DIR_TEMP,
                                         base::DIR_HOME};
  std::vector<uint32_t> total_space_vector, free_space_vector;
  uint32_t sum = 0;
  for (const auto& path_key : kPathKeys) {
    base::FilePath path;
    if (base::PathService::Get(path_key, &path)) {
      uint32_t total_space = static_cast<uint32_t>(
          base::SysInfo::AmountOfTotalDiskSpace(path) / 1024 / 1024);
      uint32_t free_space = static_cast<uint32_t>(
          base::SysInfo::AmountOfFreeDiskSpace(path) / 1024 / 1024);
      bool duplicated = false;
      for (size_t ii = 0; ii < total_space_vector.size(); ++ii) {
        if (total_space == total_space_vector[ii] &&
            free_space == free_space_vector[ii]) {
          duplicated = true;
          break;
        }
      }
      if (!duplicated) {
        total_space_vector.push_back(total_space);
        free_space_vector.push_back(free_space);
        sum += total_space;
      }
    }
  }
  return sum;
}

#if defined(OS_WIN)
uint32_t GetSystemCommitLimitMb() {
  PERFORMANCE_INFORMATION perf_info = {sizeof(perf_info)};
  if (::GetPerformanceInfo(&perf_info, sizeof(perf_info))) {
    uint64_t limit = perf_info.CommitLimit;
    limit *= perf_info.PageSize;
    limit /= 1024 * 1024;
    return static_cast<uint32_t>(limit);
  }
  return 0u;
}
#endif  // OS_WIN

GPUInfo* g_gpu_info_cache = nullptr;
GpuFeatureInfo* g_gpu_feature_info_cache = nullptr;

}  // namespace

GpuFeatureInfo ComputeGpuFeatureInfoWithHardwareAccelerationDisabled() {
  GpuFeatureInfo gpu_feature_info;
  gpu_feature_info.status_values[GPU_FEATURE_TYPE_ACCELERATED_2D_CANVAS] =
      kGpuFeatureStatusSoftware;
  gpu_feature_info.status_values[GPU_FEATURE_TYPE_ACCELERATED_WEBGL] =
      kGpuFeatureStatusSoftware;
  gpu_feature_info.status_values[GPU_FEATURE_TYPE_FLASH3D] =
      kGpuFeatureStatusDisabled;
  gpu_feature_info.status_values[GPU_FEATURE_TYPE_FLASH_STAGE3D] =
      kGpuFeatureStatusDisabled;
  gpu_feature_info.status_values[GPU_FEATURE_TYPE_ACCELERATED_VIDEO_DECODE] =
      kGpuFeatureStatusDisabled;
  gpu_feature_info.status_values[GPU_FEATURE_TYPE_FLASH_STAGE3D_BASELINE] =
      kGpuFeatureStatusDisabled;
  gpu_feature_info.status_values[GPU_FEATURE_TYPE_GPU_RASTERIZATION] =
      kGpuFeatureStatusDisabled;
  gpu_feature_info.status_values[GPU_FEATURE_TYPE_ACCELERATED_WEBGL2] =
      kGpuFeatureStatusSoftware;
  gpu_feature_info.status_values[GPU_FEATURE_TYPE_PROTECTED_VIDEO_DECODE] =
      kGpuFeatureStatusDisabled;
  gpu_feature_info.status_values[GPU_FEATURE_TYPE_OOP_RASTERIZATION] =
      kGpuFeatureStatusDisabled;
  gpu_feature_info.status_values[GPU_FEATURE_TYPE_ANDROID_SURFACE_CONTROL] =
      kGpuFeatureStatusDisabled;
  gpu_feature_info.status_values[GPU_FEATURE_TYPE_ACCELERATED_GL] =
      kGpuFeatureStatusDisabled;
  gpu_feature_info.status_values[GPU_FEATURE_TYPE_METAL] =
      kGpuFeatureStatusDisabled;
  gpu_feature_info.status_values[GPU_FEATURE_TYPE_VULKAN] =
      kGpuFeatureStatusDisabled;
#if DCHECK_IS_ON()
  for (int ii = 0; ii < NUMBER_OF_GPU_FEATURE_TYPES; ++ii) {
    DCHECK_NE(kGpuFeatureStatusUndefined, gpu_feature_info.status_values[ii]);
  }
#endif
  return gpu_feature_info;
}

GpuFeatureInfo ComputeGpuFeatureInfoWithNoGpu() {
  GpuFeatureInfo gpu_feature_info;
  gpu_feature_info.status_values[GPU_FEATURE_TYPE_ACCELERATED_2D_CANVAS] =
      kGpuFeatureStatusSoftware;
  gpu_feature_info.status_values[GPU_FEATURE_TYPE_ACCELERATED_WEBGL] =
      kGpuFeatureStatusDisabled;
  gpu_feature_info.status_values[GPU_FEATURE_TYPE_FLASH3D] =
      kGpuFeatureStatusDisabled;
  gpu_feature_info.status_values[GPU_FEATURE_TYPE_FLASH_STAGE3D] =
      kGpuFeatureStatusDisabled;
  gpu_feature_info.status_values[GPU_FEATURE_TYPE_ACCELERATED_VIDEO_DECODE] =
      kGpuFeatureStatusDisabled;
  gpu_feature_info.status_values[GPU_FEATURE_TYPE_FLASH_STAGE3D_BASELINE] =
      kGpuFeatureStatusDisabled;
  gpu_feature_info.status_values[GPU_FEATURE_TYPE_GPU_RASTERIZATION] =
      kGpuFeatureStatusDisabled;
  gpu_feature_info.status_values[GPU_FEATURE_TYPE_ACCELERATED_WEBGL2] =
      kGpuFeatureStatusDisabled;
  gpu_feature_info.status_values[GPU_FEATURE_TYPE_PROTECTED_VIDEO_DECODE] =
      kGpuFeatureStatusDisabled;
  gpu_feature_info.status_values[GPU_FEATURE_TYPE_OOP_RASTERIZATION] =
      kGpuFeatureStatusDisabled;
  gpu_feature_info.status_values[GPU_FEATURE_TYPE_ANDROID_SURFACE_CONTROL] =
      kGpuFeatureStatusDisabled;
  gpu_feature_info.status_values[GPU_FEATURE_TYPE_ACCELERATED_GL] =
      kGpuFeatureStatusDisabled;
  gpu_feature_info.status_values[GPU_FEATURE_TYPE_METAL] =
      kGpuFeatureStatusDisabled;
  gpu_feature_info.status_values[GPU_FEATURE_TYPE_VULKAN] =
      kGpuFeatureStatusDisabled;
#if DCHECK_IS_ON()
  for (int ii = 0; ii < NUMBER_OF_GPU_FEATURE_TYPES; ++ii) {
    DCHECK_NE(kGpuFeatureStatusUndefined, gpu_feature_info.status_values[ii]);
  }
#endif
  return gpu_feature_info;
}

GpuFeatureInfo ComputeGpuFeatureInfoForSwiftShader() {
  GpuFeatureInfo gpu_feature_info;
  gpu_feature_info.status_values[GPU_FEATURE_TYPE_ACCELERATED_2D_CANVAS] =
      kGpuFeatureStatusSoftware;
  gpu_feature_info.status_values[GPU_FEATURE_TYPE_ACCELERATED_WEBGL] =
      kGpuFeatureStatusSoftware;
  gpu_feature_info.status_values[GPU_FEATURE_TYPE_FLASH3D] =
      kGpuFeatureStatusDisabled;
  gpu_feature_info.status_values[GPU_FEATURE_TYPE_FLASH_STAGE3D] =
      kGpuFeatureStatusDisabled;
  gpu_feature_info.status_values[GPU_FEATURE_TYPE_ACCELERATED_VIDEO_DECODE] =
      kGpuFeatureStatusDisabled;
  gpu_feature_info.status_values[GPU_FEATURE_TYPE_FLASH_STAGE3D_BASELINE] =
      kGpuFeatureStatusDisabled;
  gpu_feature_info.status_values[GPU_FEATURE_TYPE_GPU_RASTERIZATION] =
      kGpuFeatureStatusDisabled;
  gpu_feature_info.status_values[GPU_FEATURE_TYPE_ACCELERATED_WEBGL2] =
      kGpuFeatureStatusSoftware;
  gpu_feature_info.status_values[GPU_FEATURE_TYPE_PROTECTED_VIDEO_DECODE] =
      kGpuFeatureStatusDisabled;
  gpu_feature_info.status_values[GPU_FEATURE_TYPE_OOP_RASTERIZATION] =
      kGpuFeatureStatusDisabled;
  gpu_feature_info.status_values[GPU_FEATURE_TYPE_ANDROID_SURFACE_CONTROL] =
      kGpuFeatureStatusDisabled;
  gpu_feature_info.status_values[GPU_FEATURE_TYPE_ACCELERATED_GL] =
      kGpuFeatureStatusDisabled;
  gpu_feature_info.status_values[GPU_FEATURE_TYPE_METAL] =
      kGpuFeatureStatusDisabled;
  gpu_feature_info.status_values[GPU_FEATURE_TYPE_VULKAN] =
      kGpuFeatureStatusDisabled;
#if DCHECK_IS_ON()
  for (int ii = 0; ii < NUMBER_OF_GPU_FEATURE_TYPES; ++ii) {
    DCHECK_NE(kGpuFeatureStatusUndefined, gpu_feature_info.status_values[ii]);
  }
#endif
  return gpu_feature_info;
}

GpuFeatureInfo ComputeGpuFeatureInfo(const GPUInfo& gpu_info,
                                     const GpuPreferences& gpu_preferences,
                                     base::CommandLine* command_line,
                                     bool* needs_more_info) {
  DCHECK(!needs_more_info || !(*needs_more_info));
  bool use_swift_shader = false;
  if (command_line->HasSwitch(switches::kUseGL)) {
    std::string use_gl = command_line->GetSwitchValueASCII(switches::kUseGL);
    if (use_gl == gl::kGLImplementationSwiftShaderName)
      use_swift_shader = true;
    else if (use_gl == gl::kGLImplementationANGLEName) {
      if (command_line->HasSwitch(switches::kUseANGLE)) {
        std::string use_angle =
            command_line->GetSwitchValueASCII(switches::kUseANGLE);
        if (use_angle == gl::kANGLEImplementationSwiftShaderName)
          use_swift_shader = true;
      }
    } else if (use_gl == gl::kGLImplementationSwiftShaderForWebGLName)
      return ComputeGpuFeatureInfoForSwiftShader();
    else if (use_gl == gl::kGLImplementationDisabledName)
      return ComputeGpuFeatureInfoWithNoGpu();
  }
  if (gpu_preferences.use_vulkan ==
      gpu::VulkanImplementationName::kSwiftshader) {
    use_swift_shader = true;
  }

  GpuFeatureInfo gpu_feature_info;
  std::set<int> blacklisted_features;
  if (!gpu_preferences.ignore_gpu_blacklist &&
      !command_line->HasSwitch(switches::kUseGpuInTests)) {
    std::unique_ptr<GpuBlocklist> list(GpuBlocklist::Create());
    if (gpu_preferences.log_gpu_control_list_decisions)
      list->EnableControlListLogging("gpu_blacklist");
    unsigned target_test_group = 0u;
    if (command_line->HasSwitch(switches::kGpuBlacklistTestGroup)) {
      std::string test_group_string =
          command_line->GetSwitchValueASCII(switches::kGpuBlacklistTestGroup);
      if (!base::StringToUint(test_group_string, &target_test_group))
        target_test_group = 0u;
    }
    blacklisted_features = list->MakeDecision(
        GpuControlList::kOsAny, std::string(), gpu_info, target_test_group);
    gpu_feature_info.applied_gpu_blacklist_entries = list->GetActiveEntries();
    if (needs_more_info) {
      *needs_more_info = list->needs_more_info();
    }
  }

  gpu_feature_info.status_values[GPU_FEATURE_TYPE_GPU_RASTERIZATION] =
      GetGpuRasterizationFeatureStatus(blacklisted_features, *command_line);
  gpu_feature_info.status_values[GPU_FEATURE_TYPE_ACCELERATED_WEBGL] =
      GetWebGLFeatureStatus(blacklisted_features, use_swift_shader);
  gpu_feature_info.status_values[GPU_FEATURE_TYPE_ACCELERATED_WEBGL2] =
      GetWebGL2FeatureStatus(blacklisted_features, use_swift_shader);
  gpu_feature_info.status_values[GPU_FEATURE_TYPE_ACCELERATED_2D_CANVAS] =
      Get2DCanvasFeatureStatus(blacklisted_features, use_swift_shader);
  gpu_feature_info.status_values[GPU_FEATURE_TYPE_FLASH3D] =
      GetFlash3DFeatureStatus(blacklisted_features, use_swift_shader);
  gpu_feature_info.status_values[GPU_FEATURE_TYPE_FLASH_STAGE3D] =
      GetFlashStage3DFeatureStatus(blacklisted_features, use_swift_shader);
  gpu_feature_info.status_values[GPU_FEATURE_TYPE_FLASH_STAGE3D_BASELINE] =
      GetFlashStage3DBaselineFeatureStatus(blacklisted_features,
                                           use_swift_shader);
  gpu_feature_info.status_values[GPU_FEATURE_TYPE_ACCELERATED_VIDEO_DECODE] =
      GetAcceleratedVideoDecodeFeatureStatus(blacklisted_features,
                                             use_swift_shader);
  gpu_feature_info.status_values[GPU_FEATURE_TYPE_PROTECTED_VIDEO_DECODE] =
      GetProtectedVideoDecodeFeatureStatus(blacklisted_features, gpu_info,
                                           use_swift_shader);
  gpu_feature_info.status_values[GPU_FEATURE_TYPE_OOP_RASTERIZATION] =
      GetOopRasterizationFeatureStatus(blacklisted_features, *command_line,
                                       gpu_preferences, gpu_info);
  gpu_feature_info.status_values[GPU_FEATURE_TYPE_ANDROID_SURFACE_CONTROL] =
      GetAndroidSurfaceControlFeatureStatus(blacklisted_features,
                                            gpu_preferences);
  gpu_feature_info.status_values[GPU_FEATURE_TYPE_ACCELERATED_GL] =
      GetGLFeatureStatus(blacklisted_features, use_swift_shader);
  gpu_feature_info.status_values[GPU_FEATURE_TYPE_METAL] =
      GetMetalFeatureStatus(blacklisted_features, gpu_preferences);
  gpu_feature_info.status_values[GPU_FEATURE_TYPE_VULKAN] =
      GetVulkanFeatureStatus(blacklisted_features, gpu_preferences);
#if DCHECK_IS_ON()
  for (int ii = 0; ii < NUMBER_OF_GPU_FEATURE_TYPES; ++ii) {
    DCHECK_NE(kGpuFeatureStatusUndefined, gpu_feature_info.status_values[ii]);
  }
#endif

  gfx::ExtensionSet all_disabled_extensions;
  std::string disabled_gl_extensions_value =
      command_line->GetSwitchValueASCII(switches::kDisableGLExtensions);
  if (!disabled_gl_extensions_value.empty()) {
    std::vector<base::StringPiece> command_line_disabled_extensions =
        base::SplitStringPiece(disabled_gl_extensions_value, ", ;",
                               base::KEEP_WHITESPACE,
                               base::SPLIT_WANT_NONEMPTY);
    all_disabled_extensions.insert(command_line_disabled_extensions.begin(),
                                   command_line_disabled_extensions.end());
  }

  std::set<int> enabled_driver_bug_workarounds;
  std::vector<std::string> driver_bug_disabled_extensions;
  if (!command_line->HasSwitch(switches::kDisableGpuDriverBugWorkarounds)) {
    std::unique_ptr<gpu::GpuDriverBugList> list(GpuDriverBugList::Create());
    unsigned target_test_group = 0u;
    if (command_line->HasSwitch(switches::kGpuDriverBugListTestGroup)) {
      std::string test_group_string = command_line->GetSwitchValueASCII(
          switches::kGpuDriverBugListTestGroup);
      if (!base::StringToUint(test_group_string, &target_test_group))
        target_test_group = 0u;
    }
    enabled_driver_bug_workarounds = list->MakeDecision(
        GpuControlList::kOsAny, std::string(), gpu_info, target_test_group);
    gpu_feature_info.applied_gpu_driver_bug_list_entries =
        list->GetActiveEntries();

    driver_bug_disabled_extensions = list->GetDisabledExtensions();
    all_disabled_extensions.insert(driver_bug_disabled_extensions.begin(),
                                   driver_bug_disabled_extensions.end());

    // Disabling WebGL extensions only occurs via the blacklist, so
    // the logic is simpler.
    gfx::ExtensionSet disabled_webgl_extensions;
    std::vector<std::string> disabled_webgl_extension_list =
        list->GetDisabledWebGLExtensions();
    disabled_webgl_extensions.insert(disabled_webgl_extension_list.begin(),
                                     disabled_webgl_extension_list.end());
    gpu_feature_info.disabled_webgl_extensions =
        gfx::MakeExtensionString(disabled_webgl_extensions);
  }
  gpu::GpuDriverBugList::AppendWorkaroundsFromCommandLine(
      &enabled_driver_bug_workarounds, *command_line);

  gpu_feature_info.enabled_gpu_driver_bug_workarounds.insert(
      gpu_feature_info.enabled_gpu_driver_bug_workarounds.begin(),
      enabled_driver_bug_workarounds.begin(),
      enabled_driver_bug_workarounds.end());

  if (all_disabled_extensions.size()) {
    gpu_feature_info.disabled_extensions =
        gfx::MakeExtensionString(all_disabled_extensions);
  }

  AdjustGpuFeatureStatusToWorkarounds(&gpu_feature_info);

  // TODO(zmo): Find a better way to communicate these settings to bindings
  // initialization than commandline switches.
  AppendWorkaroundsToCommandLine(gpu_feature_info, command_line);

  return gpu_feature_info;
}

void SetKeysForCrashLogging(const GPUInfo& gpu_info) {
  const GPUInfo::GPUDevice& active_gpu = gpu_info.active_gpu();
#if !defined(OS_ANDROID)
  crash_keys::gpu_vendor_id.Set(
      base::StringPrintf("0x%04x", active_gpu.vendor_id));
  crash_keys::gpu_device_id.Set(
      base::StringPrintf("0x%04x", active_gpu.device_id));
#endif  // !OS_ANDROID
#if defined(OS_WIN)
  crash_keys::gpu_sub_sys_id.Set(
      base::StringPrintf("0x%08x", active_gpu.sub_sys_id));
  crash_keys::gpu_revision.Set(base::StringPrintf("%u", active_gpu.revision));
#endif  // OS_WIN
  crash_keys::gpu_driver_version.Set(active_gpu.driver_version);
  crash_keys::gpu_pixel_shader_version.Set(gpu_info.pixel_shader_version);
  crash_keys::gpu_vertex_shader_version.Set(gpu_info.vertex_shader_version);
  crash_keys::gpu_generation_intel.Set(
      base::StringPrintf("%d", GetIntelGpuGeneration(gpu_info)));
#if defined(OS_MACOSX)
  crash_keys::gpu_gl_version.Set(gpu_info.gl_version);
#elif defined(OS_POSIX)
  crash_keys::gpu_vendor.Set(gpu_info.gl_vendor);
  crash_keys::gpu_renderer.Set(gpu_info.gl_renderer);
#endif
}

void CacheGPUInfo(const GPUInfo& gpu_info) {
  DCHECK(!g_gpu_info_cache);
  g_gpu_info_cache = new GPUInfo;
  *g_gpu_info_cache = gpu_info;
}

bool PopGPUInfoCache(GPUInfo* gpu_info) {
  if (!g_gpu_info_cache)
    return false;
  *gpu_info = *g_gpu_info_cache;
  delete g_gpu_info_cache;
  g_gpu_info_cache = nullptr;
  return true;
}

void CacheGpuFeatureInfo(const GpuFeatureInfo& gpu_feature_info) {
  DCHECK(!g_gpu_feature_info_cache);
  g_gpu_feature_info_cache = new GpuFeatureInfo;
  *g_gpu_feature_info_cache = gpu_feature_info;
}

bool PopGpuFeatureInfoCache(GpuFeatureInfo* gpu_feature_info) {
  if (!g_gpu_feature_info_cache)
    return false;
  *gpu_feature_info = *g_gpu_feature_info_cache;
  delete g_gpu_feature_info_cache;
  g_gpu_feature_info_cache = nullptr;
  return true;
}

#if defined(OS_ANDROID)
bool InitializeGLThreadSafe(base::CommandLine* command_line,
                            const GpuPreferences& gpu_preferences,
                            GPUInfo* out_gpu_info,
                            GpuFeatureInfo* out_gpu_feature_info) {
  static base::NoDestructor<base::Lock> gl_bindings_initialization_lock;
  base::AutoLock auto_lock(*gl_bindings_initialization_lock);
  DCHECK(command_line);
  DCHECK(out_gpu_info && out_gpu_feature_info);
  bool gpu_info_cached = PopGPUInfoCache(out_gpu_info);
  bool gpu_feature_info_cached = PopGpuFeatureInfoCache(out_gpu_feature_info);
  DCHECK_EQ(gpu_info_cached, gpu_feature_info_cached);
  if (gpu_info_cached) {
    // GL bindings have already been initialized in another thread.
    DCHECK_NE(gl::kGLImplementationNone, gl::GetGLImplementation());
    return true;
  }
  if (gl::GetGLImplementation() == gl::kGLImplementationNone) {
    // Some tests initialize bindings by themselves.
    if (!gl::init::InitializeGLNoExtensionsOneOff(/*init_bindings*/ true)) {
      VLOG(1) << "gl::init::InitializeGLNoExtensionsOneOff failed";
      return false;
    }
  }
  CollectContextGraphicsInfo(out_gpu_info);
  *out_gpu_feature_info = ComputeGpuFeatureInfo(*out_gpu_info, gpu_preferences,
                                                command_line, nullptr);
  if (!out_gpu_feature_info->disabled_extensions.empty()) {
    gl::init::SetDisabledExtensionsPlatform(
        out_gpu_feature_info->disabled_extensions);
  }
  if (!gl::init::InitializeExtensionSettingsOneOffPlatform()) {
    VLOG(1) << "gl::init::InitializeExtensionSettingsOneOffPlatform failed";
    return false;
  }
  CacheGPUInfo(*out_gpu_info);
  CacheGpuFeatureInfo(*out_gpu_feature_info);
  return true;
}
#endif  // OS_ANDROID

bool EnableSwiftShaderIfNeeded(base::CommandLine* command_line,
                               const GpuFeatureInfo& gpu_feature_info,
                               bool disable_software_rasterizer,
                               bool blacklist_needs_more_info) {
#if BUILDFLAG(ENABLE_SWIFTSHADER)
  if (disable_software_rasterizer || blacklist_needs_more_info)
    return false;
  // Don't overwrite user preference.
  if (command_line->HasSwitch(switches::kUseGL))
    return false;
  if (gpu_feature_info.status_values[GPU_FEATURE_TYPE_ACCELERATED_WEBGL] !=
          kGpuFeatureStatusEnabled ||
      gpu_feature_info.status_values[GPU_FEATURE_TYPE_ACCELERATED_GL] !=
          kGpuFeatureStatusEnabled) {
    command_line->AppendSwitchASCII(
        switches::kUseGL, gl::kGLImplementationSwiftShaderForWebGLName);
    return true;
  }
  return false;
#else
  return false;
#endif
}

IntelGpuSeriesType GetIntelGpuSeriesType(uint32_t vendor_id,
                                         uint32_t device_id) {
  // Note that this function's output should only depend on vendor_id and
  // device_id of a GPU. This is because we record a histogram on the output
  // and we don't want to expose an extra bit other than the already recorded
  // vendor_id and device_id.
  if (vendor_id == 0x8086) {  // Intel
    // The device IDs of Intel GPU are based on following Mesa source files:
    // include/pci_ids/i965_pci_ids.h and iris_pci_ids.h
    uint32_t masked_device_id = device_id & 0xFF00;
    switch (masked_device_id) {
      case 0x2900:
        return IntelGpuSeriesType::kBroadwater;
      case 0x2A00:
        if (device_id == 0x2A02 || device_id == 0x2A12)
          return IntelGpuSeriesType::kBroadwater;
        if (device_id == 0x2A42)
          return IntelGpuSeriesType::kEaglelake;
        break;
      case 0x2E00:
        return IntelGpuSeriesType::kEaglelake;
      case 0x0000:
        return IntelGpuSeriesType::kIronlake;
      case 0x0100:
        if (device_id == 0x0152 || device_id == 0x0156 || device_id == 0x015A ||
            device_id == 0x0162 || device_id == 0x0166 || device_id == 0x016A)
          return IntelGpuSeriesType::kIvybridge;
        if (device_id == 0x0155 || device_id == 0x0157)
          return IntelGpuSeriesType::kBaytrail;
        return IntelGpuSeriesType::kSandybridge;
      case 0x0F00:
        return IntelGpuSeriesType::kBaytrail;
      case 0x0A00:
        if (device_id == 0x0A84)
          return IntelGpuSeriesType::kApollolake;
        return IntelGpuSeriesType::kHaswell;
      case 0x0400:
      case 0x0C00:
      case 0x0D00:
        return IntelGpuSeriesType::kHaswell;
      case 0x2200:
        return IntelGpuSeriesType::kCherrytrail;
      case 0x1600:
        return IntelGpuSeriesType::kBroadwell;
      case 0x5A00:
        if (device_id == 0x5A85 || device_id == 0x5A84)
          return IntelGpuSeriesType::kApollolake;
        return IntelGpuSeriesType::kCannonlake;
      case 0x1900:
        return IntelGpuSeriesType::kSkylake;
      case 0x1A00:
        return IntelGpuSeriesType::kApollolake;
      case 0x3100:
        return IntelGpuSeriesType::kGeminilake;
      case 0x5900:
        return IntelGpuSeriesType::kKabylake;
      case 0x8700:
        if (device_id == 0x87C0)
          return IntelGpuSeriesType::kKabylake;
        if (device_id == 0x87CA)
          return IntelGpuSeriesType::kCoffeelake;
        break;
      case 0x3E00:
        if (device_id == 0x3EA0 || device_id == 0x3EA1 || device_id == 0x3EA2
            || device_id == 0x3EA4 || device_id == 0x3EA3)
          return IntelGpuSeriesType::kWhiskeylake;
        return IntelGpuSeriesType::kCoffeelake;
      case 0x9B00:
        return IntelGpuSeriesType::kCometlake;
      case 0x8A00:
        return IntelGpuSeriesType::kIcelake;
      case 0x4500:
        return IntelGpuSeriesType::kElkhartlake;
      case 0x4E00:
        return IntelGpuSeriesType::kJasperlake;
      case 0x9A00:
        return IntelGpuSeriesType::kTigerlake;
      default:
        break;
    }
  }
  return IntelGpuSeriesType::kUnknown;
}

std::string GetIntelGpuGeneration(uint32_t vendor_id, uint32_t device_id) {
  if (vendor_id == 0x8086) {
    IntelGpuSeriesType gpu_series = GetIntelGpuSeriesType(vendor_id, device_id);
    switch (gpu_series) {
      case IntelGpuSeriesType::kBroadwater:
      case IntelGpuSeriesType::kEaglelake:
        return "4";
      case IntelGpuSeriesType::kIronlake:
        return "5";
      case IntelGpuSeriesType::kSandybridge:
        return "6";
      case IntelGpuSeriesType::kBaytrail:
      case IntelGpuSeriesType::kIvybridge:
      case IntelGpuSeriesType::kHaswell:
        return "7";
      case IntelGpuSeriesType::kCherrytrail:
      case IntelGpuSeriesType::kBroadwell:
        return "8";
      case IntelGpuSeriesType::kApollolake:
      case IntelGpuSeriesType::kSkylake:
      case IntelGpuSeriesType::kGeminilake:
      case IntelGpuSeriesType::kKabylake:
      case IntelGpuSeriesType::kCoffeelake:
      case IntelGpuSeriesType::kWhiskeylake:
      case IntelGpuSeriesType::kCometlake:
        return "9";
      case IntelGpuSeriesType::kCannonlake:
        return "10";
      case IntelGpuSeriesType::kIcelake:
      case IntelGpuSeriesType::kElkhartlake:
      case IntelGpuSeriesType::kJasperlake:
        return "11";
      case IntelGpuSeriesType::kTigerlake:
        return "12";
      default:
        break;
    }
  }
  return "";
}

IntelGpuGeneration GetIntelGpuGeneration(const GPUInfo& gpu_info) {
  const uint32_t kIntelVendorId = 0x8086;
  IntelGpuGeneration latest = IntelGpuGeneration::kNonIntel;
  std::vector<uint32_t> intel_device_ids;
  if (gpu_info.gpu.vendor_id == kIntelVendorId)
    intel_device_ids.push_back(gpu_info.gpu.device_id);
  for (const auto& gpu : gpu_info.secondary_gpus) {
    if (gpu.vendor_id == kIntelVendorId)
      intel_device_ids.push_back(gpu.device_id);
  }
  if (intel_device_ids.empty())
    return latest;
  latest = IntelGpuGeneration::kUnknownIntel;
  for (uint32_t device_id : intel_device_ids) {
    std::string gen_str = gpu::GetIntelGpuGeneration(kIntelVendorId, device_id);
    int gen_int = 0;
    if (gen_str.empty() || !base::StringToInt(gen_str, &gen_int))
      continue;
    DCHECK_GE(gen_int, static_cast<int>(IntelGpuGeneration::kUnknownIntel));
    DCHECK_LE(gen_int, static_cast<int>(IntelGpuGeneration::kMaxValue));
    if (gen_int > static_cast<int>(latest))
      latest = static_cast<IntelGpuGeneration>(gen_int);
  }
  return latest;
}

void CollectDevicePerfInfo(DevicePerfInfo* device_perf_info,
                           bool in_browser_process) {
  DCHECK(device_perf_info);
  device_perf_info->total_physical_memory_mb =
      static_cast<uint32_t>(base::SysInfo::AmountOfPhysicalMemoryMB());
  if (!in_browser_process)
    device_perf_info->total_disk_space_mb = EstimateAmountOfTotalDiskSpaceMB();
  device_perf_info->hardware_concurrency =
      static_cast<uint32_t>(std::thread::hardware_concurrency());

#if defined(OS_WIN)
  device_perf_info->system_commit_limit_mb = GetSystemCommitLimitMb();
  if (!in_browser_process) {
    D3D_FEATURE_LEVEL d3d11_feature_level = D3D_FEATURE_LEVEL_1_0_CORE;
    bool has_discrete_gpu = false;
    if (CollectD3D11FeatureInfo(&d3d11_feature_level, &has_discrete_gpu)) {
      device_perf_info->d3d11_feature_level = d3d11_feature_level;
      device_perf_info->has_discrete_gpu =
          has_discrete_gpu ? HasDiscreteGpu::kYes : HasDiscreteGpu::kNo;
    }
  }
#endif
}

void RecordDevicePerfInfoHistograms() {
  base::Optional<DevicePerfInfo> device_perf_info = GetDevicePerfInfo();
  if (!device_perf_info.has_value())
    return;
  UMA_HISTOGRAM_COUNTS_1000("Hardware.TotalDiskSpace",
                            device_perf_info->total_disk_space_mb / 1024);
  UMA_HISTOGRAM_COUNTS_100("Hardware.Concurrency",
                           device_perf_info->hardware_concurrency);
#if defined(OS_WIN)
  UMA_HISTOGRAM_COUNTS_100("Memory.Total.SystemCommitLimit",
                           device_perf_info->system_commit_limit_mb / 1024);
  UMA_HISTOGRAM_ENUMERATION("GPU.D3D11FeatureLevel",
                            ConvertToHistogramD3D11FeatureLevel(
                                device_perf_info->d3d11_feature_level));
  UMA_HISTOGRAM_ENUMERATION("GPU.HasDiscreteGpu",
                            device_perf_info->has_discrete_gpu);
#endif  // OS_WIN
  UMA_HISTOGRAM_ENUMERATION("GPU.IntelGpuGeneration",
                            device_perf_info->intel_gpu_generation);
  UMA_HISTOGRAM_BOOLEAN("GPU.SoftwareRendering",
                        device_perf_info->software_rendering);
}

#if defined(OS_WIN)
std::string D3DFeatureLevelToString(uint32_t d3d_feature_level) {
  if (d3d_feature_level == 0) {
    return "Not supported";
  } else {
    return base::StringPrintf("D3D %d.%d", (d3d_feature_level >> 12) & 0xF,
                              (d3d_feature_level >> 8) & 0xF);
  }
}

std::string VulkanVersionToString(uint32_t vulkan_version) {
  if (vulkan_version == 0) {
    return "Not supported";
  } else {
    // Vulkan version number VK_MAKE_VERSION(major, minor, patch)
    // (((major) << 22) | ((minor) << 12) | (patch))
    return base::StringPrintf(
        "Vulkan API %d.%d.%d", (vulkan_version >> 22) & 0x3FF,
        (vulkan_version >> 12) & 0x3FF, vulkan_version & 0xFFF);
  }
}
#endif  // OS_WIN

VulkanVersion ConvertToHistogramVulkanVersion(uint32_t vulkan_version) {
  if (vulkan_version < VK_MAKE_VERSION(1, 0, 0))
    return VulkanVersion::kVulkanVersionUnknown;
  else if (vulkan_version < VK_MAKE_VERSION(1, 1, 0))
    return VulkanVersion::kVulkanVersion_1_0_0;
  else if (vulkan_version < VK_MAKE_VERSION(1, 2, 0))
    return VulkanVersion::kVulkanVersion_1_1_0;
  else if (vulkan_version < VK_MAKE_VERSION(1, 3, 0))
    return VulkanVersion::kVulkanVersion_1_2_0;
  else {
    // Need to add 1.3.0+ to enum VulkanVersion.
    NOTREACHED();
    return VulkanVersion::kVulkanVersion_1_2_0;
  }
}

}  // namespace gpu
