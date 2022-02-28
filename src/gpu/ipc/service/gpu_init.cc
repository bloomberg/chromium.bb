// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/ipc/service/gpu_init.h"

#include <string>

#include "base/command_line.h"
#include "base/logging.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/pattern.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "build/chromecast_buildflags.h"
#include "build/chromeos_buildflags.h"
#include "gpu/command_buffer/common/gpu_memory_buffer_support.h"
#include "gpu/command_buffer/service/gpu_switches.h"
#include "gpu/command_buffer/service/service_utils.h"
#include "gpu/config/gpu_driver_bug_list.h"
#include "gpu/config/gpu_driver_bug_workaround_type.h"
#include "gpu/config/gpu_finch_features.h"
#include "gpu/config/gpu_info_collector.h"
#include "gpu/config/gpu_switches.h"
#include "gpu/config/gpu_switching.h"
#include "gpu/config/gpu_util.h"
#include "gpu/ipc/service/gpu_watchdog_thread.h"
#include "ui/base/ui_base_features.h"
#include "ui/gfx/switches.h"
#include "ui/gl/buildflags.h"
#include "ui/gl/gl_implementation.h"
#include "ui/gl/gl_surface.h"
#include "ui/gl/gl_switches.h"
#include "ui/gl/gl_utils.h"
#include "ui/gl/init/gl_factory.h"

#if defined(OS_MAC)
#include <GLES2/gl2.h>
#endif

#if defined(USE_OZONE)
#include "ui/ozone/public/ozone_platform.h"
#include "ui/ozone/public/surface_factory_ozone.h"
#endif

#if defined(OS_WIN)
#include "gpu/config/gpu_driver_bug_workarounds.h"
#include "ui/gl/direct_composition_surface_win.h"
#include "ui/gl/gl_surface_egl.h"
#endif

#if defined(OS_ANDROID)
#include "base/android/android_image_reader_compat.h"
#include "ui/gfx/android/android_surface_control_compat.h"
#endif

#if BUILDFLAG(ENABLE_VULKAN)
#include "gpu/vulkan/init/vulkan_factory.h"
#include "gpu/vulkan/vulkan_implementation.h"
#include "gpu/vulkan/vulkan_instance.h"
#include "gpu/vulkan/vulkan_util.h"
#endif

#if defined(USE_EGL) && !defined(OS_MAC)
#include "ui/gl/gl_fence_egl.h"
#endif

namespace gpu {

namespace {
bool CollectGraphicsInfo(GPUInfo* gpu_info) {
  DCHECK(gpu_info);
  TRACE_EVENT0("gpu,startup", "Collect Graphics Info");
  base::TimeTicks before_collect_context_graphics_info = base::TimeTicks::Now();
  bool success = CollectContextGraphicsInfo(gpu_info);
  if (!success)
    LOG(ERROR) << "CollectGraphicsInfo failed.";

  if (success) {
    base::TimeDelta collect_context_time =
        base::TimeTicks::Now() - before_collect_context_graphics_info;
    UMA_HISTOGRAM_TIMES("GPU.CollectContextGraphicsInfo", collect_context_time);
  }
  return success;
}

void InitializePlatformOverlaySettings(GPUInfo* gpu_info,
                                       const GpuFeatureInfo& gpu_feature_info) {
#if defined(OS_WIN)
  // This has to be called after a context is created, active GPU is identified,
  // and GPU driver bug workarounds are computed again. Otherwise the workaround
  // |disable_direct_composition| may not be correctly applied.
  // Also, this has to be called after falling back to SwiftShader decision is
  // finalized because this function depends on GL is ANGLE's GLES or not.
  if (gpu_feature_info.IsWorkaroundEnabled(
          gpu::ENABLE_BGRA8_OVERLAYS_WITH_YUV_OVERLAY_SUPPORT)) {
    gl::DirectCompositionSurfaceWin::EnableBGRA8OverlaysWithYUVOverlaySupport();
  }
  if (gpu_feature_info.IsWorkaroundEnabled(gpu::FORCE_NV12_OVERLAY_SUPPORT)) {
    gl::DirectCompositionSurfaceWin::ForceNV12OverlaySupport();
  }
  if (gpu_feature_info.IsWorkaroundEnabled(
          gpu::FORCE_RGB10A2_OVERLAY_SUPPORT_FLAGS)) {
    gl::DirectCompositionSurfaceWin::ForceRgb10a2OverlaySupport();
  }
  if (gpu_feature_info.IsWorkaroundEnabled(
          gpu::CHECK_YCBCR_STUDIO_G22_LEFT_P709_FOR_NV12_SUPPORT)) {
    gl::DirectCompositionSurfaceWin::
        SetCheckYCbCrStudioG22LeftP709ForNv12Support();
  }

  DCHECK(gpu_info);
  CollectHardwareOverlayInfo(&gpu_info->overlay_info);
#elif defined(OS_ANDROID)
  if (gpu_info->gpu.vendor_string == "Qualcomm")
    gfx::SurfaceControl::EnableQualcommUBWC();
#endif
}

#if BUILDFLAG(IS_CHROMEOS_LACROS) || \
    (defined(OS_LINUX) && !BUILDFLAG(IS_CHROMECAST))
bool CanAccessNvidiaDeviceFile() {
  bool res = true;
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::WILL_BLOCK);
  if (access("/dev/nvidiactl", R_OK) != 0) {
    DVLOG(1) << "NVIDIA device file /dev/nvidiactl access denied";
    res = false;
  }
  return res;
}
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS) || (defined(OS_LINUX)  &&
        // !BUILDFLAG(IS_CHROMECAST))

class GpuWatchdogInit {
 public:
  GpuWatchdogInit() = default;
  ~GpuWatchdogInit() {
    if (watchdog_ptr_)
      watchdog_ptr_->OnInitComplete();
  }

  void SetGpuWatchdogPtr(GpuWatchdogThread* ptr) { watchdog_ptr_ = ptr; }

 private:
  GpuWatchdogThread* watchdog_ptr_ = nullptr;
};

// TODO(https://crbug.com/1095744): We currently do not handle
// VK_ERROR_DEVICE_LOST in in-process-gpu.
// Android WebView is allowed for now because it CHECKs on context loss.
void DisableInProcessGpuVulkan(GpuFeatureInfo* gpu_feature_info,
                               GpuPreferences* gpu_preferences) {
  if (gpu_feature_info->status_values[GPU_FEATURE_TYPE_VULKAN] ==
      kGpuFeatureStatusEnabled) {
    LOG(ERROR) << "Vulkan not supported with in process gpu";
    gpu_preferences->use_vulkan = VulkanImplementationName::kNone;
    gpu_feature_info->status_values[GPU_FEATURE_TYPE_VULKAN] =
        kGpuFeatureStatusDisabled;
    if (gpu_preferences->gr_context_type == GrContextType::kVulkan)
      gpu_preferences->gr_context_type = GrContextType::kGL;
  }
}

#if BUILDFLAG(ENABLE_VULKAN)
bool MatchGLRenderer(const GPUInfo& gpu_info, const std::string& patterns) {
  auto pattern_strings = base::SplitString(patterns, "|", base::TRIM_WHITESPACE,
                                           base::SPLIT_WANT_ALL);
  for (const auto& pattern : pattern_strings) {
    if (base::MatchPattern(gpu_info.gl_renderer, pattern))
      return true;
  }
  return false;
}
#endif  // !BUILDFLAG(ENABLE_VULKAN)

}  // namespace

GpuInit::GpuInit() = default;

GpuInit::~GpuInit() {
  StopForceDiscreteGPU();
}

bool GpuInit::InitializeAndStartSandbox(base::CommandLine* command_line,
                                        const GpuPreferences& gpu_preferences) {
  gpu_preferences_ = gpu_preferences;
  // Blocklist decisions based on basic GPUInfo may not be final. It might
  // need more context based GPUInfo. In such situations, switching to
  // SwiftShader needs to wait until creating a context.
  bool needs_more_info = true;
#if !defined(OS_ANDROID) && !BUILDFLAG(IS_CHROMECAST)
  needs_more_info = false;
  if (!PopGPUInfoCache(&gpu_info_)) {
    CollectBasicGraphicsInfo(command_line, &gpu_info_);
  }
#if defined(OS_WIN)
  IntelGpuSeriesType intel_gpu_series_type = GetIntelGpuSeriesType(
      gpu_info_.active_gpu().vendor_id, gpu_info_.active_gpu().device_id);
  UMA_HISTOGRAM_ENUMERATION("GPU.IntelGpuSeriesType", intel_gpu_series_type);
#endif  // OS_WIN

  // Set keys for crash logging based on preliminary gpu info, in case we
  // crash during feature collection.
  SetKeysForCrashLogging(gpu_info_);
#if defined(SUBPIXEL_FONT_RENDERING_DISABLED)
  gpu_info_.subpixel_font_rendering = false;
#else
  gpu_info_.subpixel_font_rendering = true;
#endif

  if (gpu_preferences_.enable_perf_data_collection) {
    // This is only enabled on the info collection GPU process.
    DevicePerfInfo device_perf_info;
    CollectDevicePerfInfo(&device_perf_info, /*in_browser_process=*/false);
    device_perf_info_ = device_perf_info;
  }

#if defined(OS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS)
  if (gpu_info_.gpu.vendor_id == 0x10de &&  // NVIDIA
      gpu_info_.gpu.driver_vendor == "NVIDIA" && !CanAccessNvidiaDeviceFile())
    return false;
#endif
  if (!PopGpuFeatureInfoCache(&gpu_feature_info_)) {
    // Compute blocklist and driver bug workaround decisions based on basic GPU
    // info.
    gpu_feature_info_ = ComputeGpuFeatureInfo(gpu_info_, gpu_preferences_,
                                              command_line, &needs_more_info);
  }
#endif  // !OS_ANDROID && !BUILDFLAG(IS_CHROMECAST)

  gpu_info_.in_process_gpu = false;
  gl_use_swiftshader_ = false;

  // GL bindings may have already been initialized, specifically on MacOSX.
  bool gl_initialized = gl::GetGLImplementation() != gl::kGLImplementationNone;
  if (!gl_initialized) {
    // If GL has already been initialized, then it's too late to select GPU.
    if (SwitchableGPUsSupported(gpu_info_, *command_line)) {
      InitializeSwitchableGPUs(
          gpu_feature_info_.enabled_gpu_driver_bug_workarounds);
    }
  // If SwiftShader/SwANGLE is in use, set the flag gl_use_swiftshader so GPU
  // initialization will take a software rendering path. Do not do this if
  // SwiftShader/SwANGLE are explicitly requested via flags, because the flags
  // are meant to specify running SwiftShader/SwANGLE on the hardware GPU path.
  } else if (gl::GetGLImplementationParts() ==
                 gl::GetLegacySoftwareGLImplementation() &&
             command_line->GetSwitchValueASCII(switches::kUseGL) !=
                 gl::kGLImplementationSwiftShaderName) {
    gl_use_swiftshader_ = true;
  } else if (gl::GetGLImplementationParts() ==
                 gl::GetSoftwareGLImplementation() &&
             (command_line->GetSwitchValueASCII(switches::kUseGL) !=
                  gl::kGLImplementationANGLEName ||
              command_line->GetSwitchValueASCII(switches::kUseANGLE) !=
                  gl::kANGLEImplementationSwiftShaderName)) {
    gl_use_swiftshader_ = true;
  }

  bool enable_watchdog = !gpu_preferences_.disable_gpu_watchdog &&
                         !command_line->HasSwitch(switches::kHeadless) &&
                         !gl_use_swiftshader_;

  // Disable the watchdog in debug builds because they tend to only be run by
  // developers who will not appreciate the watchdog killing the GPU process.
#ifndef NDEBUG
  enable_watchdog = false;
#endif

  // watchdog_init will call watchdog OnInitComplete() at the end of this
  // function.
  GpuWatchdogInit watchdog_init;

  bool delayed_watchdog_enable = false;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Don't start watchdog immediately, to allow developers to switch to VT2 on
  // startup.
  delayed_watchdog_enable = true;
#endif

#if defined(OS_LINUX) || defined(OS_CHROMEOS)
  // PreSandbox is mainly for resource handling and not related to the GPU
  // driver, it doesn't need the GPU watchdog. The loadLibrary may take long
  // time that killing and restarting the GPU process will not help.
  if (gpu_preferences_.gpu_sandbox_start_early) {
    // The sandbox will be started earlier than usual (i.e. before GL) so
    // execute the pre-sandbox steps now.
    sandbox_helper_->PreSandboxStartup(gpu_preferences);
  }
#else
  // For some reasons MacOSX's VideoToolbox might crash when called after
  // initializing GL, see crbug.com/1047643 and crbug.com/871280. On other
  // operating systems like Windows and Android the pre-sandbox steps have
  // always been executed before initializing GL so keep it this way.
  sandbox_helper_->PreSandboxStartup(gpu_preferences);
#endif

  // Start the GPU watchdog only after anything that is expected to be time
  // consuming has completed, otherwise the process is liable to be aborted.
  if (enable_watchdog && !delayed_watchdog_enable) {
    watchdog_thread_ = GpuWatchdogThread::Create(
        gpu_preferences_.watchdog_starts_backgrounded, "GpuWatchdog");
    watchdog_init.SetGpuWatchdogPtr(watchdog_thread_.get());

#if defined(OS_WIN)
    // This is a workaround for an occasional deadlock between watchdog and
    // current thread. Watchdog hangs at thread initialization in
    // __acrt_thread_attach() and current thread in std::setlocale(...)
    // (during InitializeGLOneOff()). Source of the deadlock looks like an old
    // UCRT bug that was supposed to be fixed in 10.0.10586 release of UCRT,
    // but we might have come accross a not-yet-covered scenario.
    // References:
    // https://bugs.python.org/issue26624
    // http://stackoverflow.com/questions/35572792/setlocale-stuck-on-windows
    auto watchdog_started = watchdog_thread_->WaitUntilThreadStarted();
    DCHECK(watchdog_started);
#endif  // OS_WIN
  }

  bool attempted_startsandbox = false;
#if defined(OS_LINUX) || defined(OS_CHROMEOS)
  // On Chrome OS ARM Mali, GPU driver userspace creates threads when
  // initializing a GL context, so start the sandbox early.
  // TODO(zmo): Need to collect OS version before this.
  if (gpu_preferences_.gpu_sandbox_start_early) {
    gpu_info_.sandboxed = sandbox_helper_->EnsureSandboxInitialized(
        watchdog_thread_.get(), &gpu_info_, gpu_preferences_);
    attempted_startsandbox = true;
  }
#endif  // defined(OS_LINUX) || defined(OS_CHROMEOS)

  base::TimeTicks before_initialize_one_off = base::TimeTicks::Now();

#if defined(USE_OZONE)
  // Initialize Ozone GPU after the watchdog in case it hangs. The sandbox
  // may also have started at this point.
  std::vector<gfx::BufferFormat> supported_buffer_formats_for_texturing;
  ui::OzonePlatform::InitParams params;
  params.single_process = false;
  params.enable_native_gpu_memory_buffers =
      gpu_preferences.enable_native_gpu_memory_buffers;

  // Page flip testing will only happen in ash-chrome, not in lacros-chrome.
  // Therefore, we only allow or disallow sync and real buffer page flip
  // testing for ash-chrome.
#if BUILDFLAG(IS_CHROMEOS_ASH)
#if BUILDFLAG(USE_CHROMEOS_MEDIA_ACCELERATION)
  params.allow_sync_and_real_buffer_page_flip_testing =
      gpu_preferences_.enable_chromeos_direct_video_decoder;
#else   // !BUILDFLAG(USE_CHROMEOS_MEDIA_ACCELERATION)
  params.allow_sync_and_real_buffer_page_flip_testing = true;
#endif  // BUILDFLAG(USE_CHROMEOS_MEDIA_ACCELERATION)
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  ui::OzonePlatform::InitializeForGPU(params);
  // We need to get supported formats before sandboxing to avoid an known
  // issue which breaks the camera preview. (b/166850715)
  supported_buffer_formats_for_texturing =
      ui::OzonePlatform::GetInstance()
          ->GetSurfaceFactoryOzone()
          ->GetSupportedFormatsForTexturing();
#endif

  if (!gl_use_swiftshader_) {
    gl_use_swiftshader_ = EnableSwiftShaderIfNeeded(
        command_line, gpu_feature_info_,
        gpu_preferences_.disable_software_rasterizer, needs_more_info);
  }
  if (gl_initialized && gl_use_swiftshader_ &&
      !gl::IsSoftwareGLImplementation(gl::GetGLImplementationParts())) {
#if defined(OS_LINUX) || defined(OS_CHROMEOS)
    VLOG(1) << "Quit GPU process launch to fallback to SwiftShader cleanly "
            << "on Linux";
    return false;
#else
    SaveHardwareGpuInfoAndGpuFeatureInfo();
    gl::init::ShutdownGL(true);
    gl_initialized = false;
#endif  // defined(OS_LINUX) || defined(OS_CHROMEOS)
  }

  if (!gl_initialized) {
    // Pause watchdog. LoadLibrary in GLBindings may take long time.
    if (watchdog_thread_)
      watchdog_thread_->PauseWatchdog();
    gl_initialized = gl::init::InitializeStaticGLBindingsOneOff();

    if (!gl_initialized) {
      VLOG(1) << "gl::init::InitializeStaticGLBindingsOneOff failed";
      return false;
    }

    if (watchdog_thread_)
      watchdog_thread_->ResumeWatchdog();
    if (gl::GetGLImplementation() != gl::kGLImplementationDisabled) {
      gl_initialized =
          gl::init::InitializeGLNoExtensionsOneOff(/*init_bindings*/ false);
      if (!gl_initialized) {
        VLOG(1) << "gl::init::InitializeGLNoExtensionsOneOff failed";
        return false;
      }
    }
  }

#if defined(OS_LINUX) || defined(OS_CHROMEOS)
  // The ContentSandboxHelper is currently the only one implementation of
  // GpuSandboxHelper and it has no dependency. Except on Linux where
  // VaapiWrapper checks the GL implementation to determine which display
  // to use. So call PreSandboxStartup after GL initialization. But make
  // sure the watchdog is paused as loadLibrary may take a long time and
  // restarting the GPU process will not help.
  if (!attempted_startsandbox) {
    if (watchdog_thread_)
      watchdog_thread_->PauseWatchdog();

    // The sandbox is not started yet.
    sandbox_helper_->PreSandboxStartup(gpu_preferences);

    if (watchdog_thread_)
      watchdog_thread_->ResumeWatchdog();
  }
#endif

  // On MacOS, the default texture target for native GpuMemoryBuffers is
  // GL_TEXTURE_RECTANGLE_ARB. This is due to CGL's requirements for creating
  // a GL surface. However, when ANGLE is used on top of SwiftShader or Metal,
  // it's necessary to use GL_TEXTURE_2D instead.
  // TODO(crbug.com/1056312): The proper behavior is to check the config
  // parameter set by the EGL_ANGLE_iosurface_client_buffer extension
#if defined(OS_MAC)
  if (gl::GetGLImplementation() == gl::kGLImplementationEGLANGLE &&
      (gl::GetANGLEImplementation() == gl::ANGLEImplementation::kSwiftShader ||
       gl::GetANGLEImplementation() == gl::ANGLEImplementation::kMetal)) {
    SetMacOSSpecificTextureTarget(GL_TEXTURE_2D);
  }
#endif  // defined(OS_MAC)

  bool gl_disabled = gl::GetGLImplementation() == gl::kGLImplementationDisabled;

  // Compute passthrough decoder status before ComputeGpuFeatureInfo below.
  // Do this after GL is initialized so extensions can be queried.
  if (gles2::UsePassthroughCommandDecoder(command_line)) {
    gpu_info_.passthrough_cmd_decoder =
        gles2::PassthroughCommandDecoderSupported();
#if defined(OS_ANDROID)
    // We never use swiftshader on Android
    LOG_IF(DFATAL, !gpu_info_.passthrough_cmd_decoder)
#else
    LOG_IF(ERROR, !gpu_info_.passthrough_cmd_decoder)
#endif
        << "Passthrough is not supported, GL is "
        << gl::GetGLImplementationGLName(gl::GetGLImplementationParts())
        << ", ANGLE is "
        << gl::GetGLImplementationANGLEName(gl::GetGLImplementationParts());
  } else {
    gpu_info_.passthrough_cmd_decoder = false;
  }

  // We need to collect GL strings (VENDOR, RENDERER) for blocklisting purposes.
  if (!gl_disabled) {
    if (!gl_use_swiftshader_) {
      if (!CollectGraphicsInfo(&gpu_info_))
        return false;

      SetKeysForCrashLogging(gpu_info_);
      gpu_feature_info_ = ComputeGpuFeatureInfo(gpu_info_, gpu_preferences_,
                                                command_line, nullptr);
      gl_use_swiftshader_ = EnableSwiftShaderIfNeeded(
          command_line, gpu_feature_info_,
          gpu_preferences_.disable_software_rasterizer, false);
      if (gl_use_swiftshader_) {
#if defined(OS_LINUX) || defined(OS_CHROMEOS)
        VLOG(1) << "Quit GPU process launch to fallback to SwiftShader cleanly "
                << "on Linux";
        return false;
#else
        SaveHardwareGpuInfoAndGpuFeatureInfo();
        gl::init::ShutdownGL(true);
        watchdog_thread_ = nullptr;
        watchdog_init.SetGpuWatchdogPtr(nullptr);
        if (!gl::init::InitializeGLNoExtensionsOneOff(/*init_bindings*/ true)) {
          VLOG(1)
              << "gl::init::InitializeGLNoExtensionsOneOff with SwiftShader "
              << "failed";
          return false;
        }
#endif  // defined(OS_LINUX) || defined(OS_CHROMEOS)
      }
    } else {  // gl_use_swiftshader_ == true
      switch (gpu_preferences_.use_vulkan) {
        case VulkanImplementationName::kNative: {
          // Collect GPU info, so we can use blocklist to disable vulkan if it
          // is needed.
          GPUInfo gpu_info;
          if (!CollectGraphicsInfo(&gpu_info))
            return false;
          auto gpu_feature_info = ComputeGpuFeatureInfo(
              gpu_info, gpu_preferences_, command_line, nullptr);
          gpu_feature_info_.status_values[GPU_FEATURE_TYPE_VULKAN] =
              gpu_feature_info.status_values[GPU_FEATURE_TYPE_VULKAN];
          break;
        }
        case VulkanImplementationName::kForcedNative:
        case VulkanImplementationName::kSwiftshader:
          gpu_feature_info_.status_values[GPU_FEATURE_TYPE_VULKAN] =
              kGpuFeatureStatusEnabled;
          break;
        case VulkanImplementationName::kNone:
          gpu_feature_info_.status_values[GPU_FEATURE_TYPE_VULKAN] =
              kGpuFeatureStatusDisabled;
          break;
      }
    }
  }

  if (gpu_feature_info_.status_values[GPU_FEATURE_TYPE_VULKAN] !=
          kGpuFeatureStatusEnabled ||
      !InitializeVulkan()) {
    gpu_preferences_.use_vulkan = VulkanImplementationName::kNone;
    gpu_feature_info_.status_values[GPU_FEATURE_TYPE_VULKAN] =
        kGpuFeatureStatusDisabled;
    if (gpu_preferences_.gr_context_type == GrContextType::kVulkan) {
#if defined(OS_FUCHSIA)
      // Fuchsia uses ANGLE for GL which requires Vulkan, so don't fall
      // back to GL if Vulkan init fails.
      LOG(FATAL) << "Vulkan initialization failed";
#endif
      gpu_preferences_.gr_context_type = GrContextType::kGL;
    }
  } else {
    // TODO(https://crbug.com/1095744): It would be better to cleanly tear
    // down and recreate the VkDevice on VK_ERROR_DEVICE_LOST. Until that
    // happens, we will exit_on_context_lost to ensure there are no leaks.
    gpu_feature_info_.enabled_gpu_driver_bug_workarounds.push_back(
        EXIT_ON_CONTEXT_LOST);

    // Disable RGB format because ExternalVkImageBacking doesn't handle it
    // correctly (see https://crbug.com/1269826). This workaround is not
    // necessary on Android because it uses SharedImageBackingFactoryAHB.
    // TODO(https://crbug.com/1269826): Remove once RGBX support is fixed in
    // ExternalVkImageBacking.
#if !defined(OS_ANDROID)
    gpu_feature_info_.enabled_gpu_driver_bug_workarounds.push_back(
        DISABLE_GL_RGB_FORMAT);
#endif
  }

  // Collect GPU process info
  if (!gl_disabled) {
    if (!CollectGpuExtraInfo(&gpu_extra_info_, gpu_preferences))
      return false;
  }

  if (!gl_disabled) {
    if (!gpu_feature_info_.disabled_extensions.empty()) {
      gl::init::SetDisabledExtensionsPlatform(
          gpu_feature_info_.disabled_extensions);
    }
    if (!gl::init::InitializeExtensionSettingsOneOffPlatform()) {
      VLOG(1) << "gl::init::InitializeExtensionSettingsOneOffPlatform failed";
      return false;
    }
    default_offscreen_surface_ =
        gl::init::CreateOffscreenGLSurface(gfx::Size());
    if (!default_offscreen_surface_) {
      VLOG(1) << "gl::init::CreateOffscreenGLSurface failed";
      return false;
    }
  }

  InitializePlatformOverlaySettings(&gpu_info_, gpu_feature_info_);

#if defined(OS_LINUX) || defined(OS_CHROMEOS)
  // Driver may create a compatibility profile context when collect graphics
  // information on Linux platform. Try to collect graphics information
  // based on core profile context after disabling platform extensions.
  if (!gl_disabled && !gl_use_swiftshader_) {
    if (!CollectGraphicsInfo(&gpu_info_))
      return false;
    SetKeysForCrashLogging(gpu_info_);
    gpu_feature_info_ = ComputeGpuFeatureInfo(gpu_info_, gpu_preferences_,
                                              command_line, nullptr);
    gl_use_swiftshader_ = EnableSwiftShaderIfNeeded(
        command_line, gpu_feature_info_,
        gpu_preferences_.disable_software_rasterizer, false);
    if (gl_use_swiftshader_) {
      VLOG(1) << "Quit GPU process launch to fallback to SwiftShader cleanly "
              << "on Linux";
      return false;
    }
  }
#endif  // defined(OS_LINUX) || defined(OS_CHROMEOS)

  if (gl_use_swiftshader_) {
    AdjustInfoToSwiftShader();
  }

  if (kGpuFeatureStatusEnabled !=
      gpu_feature_info_
          .status_values[GPU_FEATURE_TYPE_ACCELERATED_VIDEO_DECODE]) {
    gpu_preferences_.disable_accelerated_video_decode = true;
  }

  if (kGpuFeatureStatusEnabled !=
      gpu_feature_info_
          .status_values[GPU_FEATURE_TYPE_ACCELERATED_VIDEO_ENCODE]) {
    gpu_preferences_.disable_accelerated_video_encode = true;
  }

  base::TimeDelta initialize_one_off_time =
      base::TimeTicks::Now() - before_initialize_one_off;
  UMA_HISTOGRAM_MEDIUM_TIMES("GPU.InitializeOneOffMediumTime",
                             initialize_one_off_time);

  // Software GL is expected to run slowly, so disable the watchdog
  // in that case.
  // In SwiftShader case, the implementation is actually EGLGLES2.
  if (!gl_use_swiftshader_ && command_line->HasSwitch(switches::kUseGL)) {
    std::string use_gl = command_line->GetSwitchValueASCII(switches::kUseGL);
    std::string use_angle =
        command_line->GetSwitchValueASCII(switches::kUseANGLE);
    if (use_gl == gl::kGLImplementationSwiftShaderName ||
        use_gl == gl::kGLImplementationSwiftShaderForWebGLName ||
        (use_gl == gl::kGLImplementationANGLEName &&
         (use_angle == gl::kANGLEImplementationSwiftShaderName ||
          use_angle == gl::kANGLEImplementationSwiftShaderForWebGLName))) {
      gl_use_swiftshader_ = true;
    }
  }
  if (gl_use_swiftshader_ ||
      gl::IsSoftwareGLImplementation(gl::GetGLImplementationParts())) {
    gpu_info_.software_rendering = true;
    watchdog_thread_ = nullptr;
    watchdog_init.SetGpuWatchdogPtr(nullptr);
  } else if (gl_disabled) {
    watchdog_thread_ = nullptr;
    watchdog_init.SetGpuWatchdogPtr(nullptr);
  } else if (enable_watchdog && delayed_watchdog_enable) {
    watchdog_thread_ = GpuWatchdogThread::Create(
        gpu_preferences_.watchdog_starts_backgrounded, "GpuWatchdog");
    watchdog_init.SetGpuWatchdogPtr(watchdog_thread_.get());
  }

  UMA_HISTOGRAM_ENUMERATION("GPU.GLImplementation", gl::GetGLImplementation());

  if (!gpu_info_.sandboxed && !attempted_startsandbox) {
    gpu_info_.sandboxed = sandbox_helper_->EnsureSandboxInitialized(
        watchdog_thread_.get(), &gpu_info_, gpu_preferences_);
  }
  UMA_HISTOGRAM_BOOLEAN("GPU.Sandbox.InitializedSuccessfully",
                        gpu_info_.sandboxed);

  init_successful_ = true;
#if defined(USE_OZONE)
  ui::OzonePlatform::GetInstance()->AfterSandboxEntry();
  gpu_feature_info_.supported_buffer_formats_for_allocation_and_texturing =
      std::move(supported_buffer_formats_for_texturing);
#endif

  if (!watchdog_thread_)
    watchdog_init.SetGpuWatchdogPtr(nullptr);

#if defined(OS_WIN)
  if (gpu_feature_info_.IsWorkaroundEnabled(DISABLE_DECODE_SWAP_CHAIN))
    gl::DirectCompositionSurfaceWin::DisableDecodeSwapChain();
  if (gpu_feature_info_.IsWorkaroundEnabled(
          DISABLE_DIRECT_COMPOSITION_SW_VIDEO_OVERLAYS)) {
    gl::DirectCompositionSurfaceWin::DisableSoftwareOverlays();
  }
#endif

#if defined(USE_EGL) && !defined(OS_MAC)
  if (gpu_feature_info_.IsWorkaroundEnabled(CHECK_EGL_FENCE_BEFORE_WAIT))
    gl::GLFenceEGL::CheckEGLFenceBeforeWait();
#endif

  return true;
}

#if defined(OS_ANDROID)
void GpuInit::InitializeInProcess(base::CommandLine* command_line,
                                  const GpuPreferences& gpu_preferences) {
  gpu_preferences_ = gpu_preferences;
  init_successful_ = true;
  DCHECK(!EnableSwiftShaderIfNeeded(
      command_line, gpu_feature_info_,
      gpu_preferences_.disable_software_rasterizer, false));

  InitializeGLThreadSafe(command_line, gpu_preferences_, &gpu_info_,
                         &gpu_feature_info_);

  if (command_line->HasSwitch(switches::kWebViewDrawFunctorUsesVulkan) &&
      base::FeatureList::IsEnabled(features::kWebViewVulkan)) {
    bool result = InitializeVulkan();
    // There is no fallback for webview.
    CHECK(result);
  } else {
    DisableInProcessGpuVulkan(&gpu_feature_info_, &gpu_preferences_);
  }
  default_offscreen_surface_ = gl::init::CreateOffscreenGLSurface(gfx::Size());

  UMA_HISTOGRAM_ENUMERATION("GPU.GLImplementation", gl::GetGLImplementation());
}
#else
void GpuInit::InitializeInProcess(base::CommandLine* command_line,
                                  const GpuPreferences& gpu_preferences) {
  gpu_preferences_ = gpu_preferences;
  init_successful_ = true;
#if defined(USE_OZONE)
  ui::OzonePlatform::InitParams params;
  params.single_process = true;

  // Page flip testing will only happen in ash-chrome, not in lacros-chrome.
  // Therefore, we only allow or disallow sync and real buffer page flip
  // testing for ash-chrome.
#if BUILDFLAG(IS_CHROMEOS_ASH)
#if BUILDFLAG(USE_CHROMEOS_MEDIA_ACCELERATION)
  params.allow_sync_and_real_buffer_page_flip_testing =
      gpu_preferences_.enable_chromeos_direct_video_decoder;
#else   // !BUILDFLAG(USE_CHROMEOS_MEDIA_ACCELERATION)
  params.allow_sync_and_real_buffer_page_flip_testing = true;
#endif  // BUILDFLAG(USE_CHROMEOS_MEDIA_ACCELERATION)
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  ui::OzonePlatform::InitializeForGPU(params);
#endif
  bool needs_more_info = true;
#if !BUILDFLAG(IS_CHROMECAST)
  needs_more_info = false;
  if (!PopGPUInfoCache(&gpu_info_)) {
    CollectBasicGraphicsInfo(command_line, &gpu_info_);
  }
#if defined(SUBPIXEL_FONT_RENDERING_DISABLED)
  gpu_info_.subpixel_font_rendering = false;
#else
  gpu_info_.subpixel_font_rendering = true;
#endif
  if (!PopGpuFeatureInfoCache(&gpu_feature_info_)) {
    gpu_feature_info_ = ComputeGpuFeatureInfo(gpu_info_, gpu_preferences_,
                                              command_line, &needs_more_info);
  }
  if (SwitchableGPUsSupported(gpu_info_, *command_line)) {
    InitializeSwitchableGPUs(
        gpu_feature_info_.enabled_gpu_driver_bug_workarounds);
  }
#endif  // !BUILDFLAG(IS_CHROMECAST)

  // On MacOS, the default texture target for native GpuMemoryBuffers is
  // GL_TEXTURE_RECTANGLE_ARB. This is due to CGL's requirements for creating
  // a GL surface. However, when ANGLE is used on top of SwiftShader or Metal,
  // it's necessary to use GL_TEXTURE_2D instead.
  // TODO(crbug.com/1056312): The proper behavior is to check the config
  // parameter set by the EGL_ANGLE_iosurface_client_buffer extension
#if defined(OS_MAC)
  if (command_line->HasSwitch(switches::kUseGL)) {
    std::string use_gl = command_line->GetSwitchValueASCII(switches::kUseGL);
    std::string use_angle =
        command_line->GetSwitchValueASCII(switches::kUseANGLE);
    if (use_gl == gl::kGLImplementationANGLEName &&
        (use_angle == gl::kANGLEImplementationSwiftShaderName ||
         use_angle == gl::kANGLEImplementationSwiftShaderForWebGLName ||
         use_angle == gl::kANGLEImplementationMetalName)) {
      SetMacOSSpecificTextureTarget(GL_TEXTURE_2D);
    }
  }
#endif  // defined(OS_MAC)

  gl_use_swiftshader_ = EnableSwiftShaderIfNeeded(
      command_line, gpu_feature_info_,
      gpu_preferences_.disable_software_rasterizer, needs_more_info);
  if (!gl::init::InitializeGLNoExtensionsOneOff(/*init_bindings*/ true)) {
    VLOG(1) << "gl::init::InitializeGLNoExtensionsOneOff failed";
    return;
  }
  bool gl_disabled = gl::GetGLImplementation() == gl::kGLImplementationDisabled;

  if (!gl_disabled && !gl_use_swiftshader_) {
    CollectContextGraphicsInfo(&gpu_info_);
    gpu_feature_info_ = ComputeGpuFeatureInfo(gpu_info_, gpu_preferences_,
                                              command_line, nullptr);
    gl_use_swiftshader_ = EnableSwiftShaderIfNeeded(
        command_line, gpu_feature_info_,
        gpu_preferences_.disable_software_rasterizer, false);
    if (gl_use_swiftshader_) {
      SaveHardwareGpuInfoAndGpuFeatureInfo();
      gl::init::ShutdownGL(true);
      if (!gl::init::InitializeGLNoExtensionsOneOff(/*init_bindings*/ true)) {
        VLOG(1) << "gl::init::InitializeGLNoExtensionsOneOff failed "
                << "with SwiftShader";
        return;
      }
    }
  }

  if (!gl_disabled) {
    if (!gpu_feature_info_.disabled_extensions.empty()) {
      gl::init::SetDisabledExtensionsPlatform(
          gpu_feature_info_.disabled_extensions);
    }
    if (!gl::init::InitializeExtensionSettingsOneOffPlatform()) {
      VLOG(1) << "gl::init::InitializeExtensionSettingsOneOffPlatform failed";
    }
    default_offscreen_surface_ =
        gl::init::CreateOffscreenGLSurface(gfx::Size());
    if (!default_offscreen_surface_) {
      VLOG(1) << "gl::init::CreateOffscreenGLSurface failed";
    }
  }

  InitializePlatformOverlaySettings(&gpu_info_, gpu_feature_info_);

#if defined(OS_LINUX) || defined(OS_CHROMEOS)
  // Driver may create a compatibility profile context when collect graphics
  // information on Linux platform. Try to collect graphics information
  // based on core profile context after disabling platform extensions.
  if (!gl_disabled && !gl_use_swiftshader_) {
    CollectContextGraphicsInfo(&gpu_info_);
    gpu_feature_info_ = ComputeGpuFeatureInfo(gpu_info_, gpu_preferences_,
                                              command_line, nullptr);
    gl_use_swiftshader_ = EnableSwiftShaderIfNeeded(
        command_line, gpu_feature_info_,
        gpu_preferences_.disable_software_rasterizer, false);
    if (gl_use_swiftshader_) {
      SaveHardwareGpuInfoAndGpuFeatureInfo();
      gl::init::ShutdownGL(true);
      if (!gl::init::InitializeGLNoExtensionsOneOff(/*init_bindings*/ true)) {
        VLOG(1) << "gl::init::InitializeGLNoExtensionsOneOff failed "
                << "with SwiftShader";
        return;
      }
    }
  }
#endif  // defined(OS_LINUX) || defined(OS_CHROMEOS)

  if (gl_use_swiftshader_) {
    AdjustInfoToSwiftShader();
  }

#if defined(USE_OZONE)
  const std::vector<gfx::BufferFormat> supported_buffer_formats_for_texturing =
      ui::OzonePlatform::GetInstance()
          ->GetSurfaceFactoryOzone()
          ->GetSupportedFormatsForTexturing();
  gpu_feature_info_.supported_buffer_formats_for_allocation_and_texturing =
      std::move(supported_buffer_formats_for_texturing);
#endif

  DisableInProcessGpuVulkan(&gpu_feature_info_, &gpu_preferences_);

#if defined(OS_WIN)
  if (gpu_feature_info_.IsWorkaroundEnabled(DISABLE_DECODE_SWAP_CHAIN))
    gl::DirectCompositionSurfaceWin::DisableDecodeSwapChain();
#endif

  UMA_HISTOGRAM_ENUMERATION("GPU.GLImplementation", gl::GetGLImplementation());
}
#endif  // OS_ANDROID

void GpuInit::SaveHardwareGpuInfoAndGpuFeatureInfo() {
  gpu_info_for_hardware_gpu_ = gpu_info_;
  gpu_feature_info_for_hardware_gpu_ = gpu_feature_info_;
}

void GpuInit::AdjustInfoToSwiftShader() {
  gpu_info_.passthrough_cmd_decoder = false;
  gpu_feature_info_ = ComputeGpuFeatureInfoForSwiftShader();
  CollectContextGraphicsInfo(&gpu_info_);

  DCHECK_EQ(gpu_info_.passthrough_cmd_decoder, false);
}

scoped_refptr<gl::GLSurface> GpuInit::TakeDefaultOffscreenSurface() {
  return std::move(default_offscreen_surface_);
}

#if BUILDFLAG(ENABLE_VULKAN)
bool GpuInit::InitializeVulkan() {
  DCHECK_EQ(gpu_feature_info_.status_values[GPU_FEATURE_TYPE_VULKAN],
            kGpuFeatureStatusEnabled);
  DCHECK_NE(gpu_preferences_.use_vulkan, VulkanImplementationName::kNone);
  bool vulkan_use_swiftshader =
      gpu_preferences_.use_vulkan == VulkanImplementationName::kSwiftshader;
  bool forced_native =
      gpu_preferences_.use_vulkan == VulkanImplementationName::kForcedNative;
  bool use_swiftshader = gl_use_swiftshader_ || vulkan_use_swiftshader;

  vulkan_implementation_ = CreateVulkanImplementation(
      vulkan_use_swiftshader, gpu_preferences_.enable_vulkan_protected_memory);
  if (!vulkan_implementation_ ||
      !vulkan_implementation_->InitializeVulkanInstance(
          !gpu_preferences_.disable_vulkan_surface)) {
    DLOG(ERROR) << "Failed to create and initialize Vulkan implementation.";
    vulkan_implementation_ = nullptr;
    CHECK(!gpu_preferences_.disable_vulkan_fallback_to_gl_for_testing);
  }

  // Vulkan info is no longer collected in gpu/config/gpu_info_collector_win.cc
  // Histogram GPU.SupportsVulkan and GPU.VulkanVersion were marked as expired.
  // TODO(magchen): Add back these two histograms here and re-enable them in
  // histograms.xml when we start Vulkan finch on Windows.

  if (!vulkan_implementation_)
    return false;

  const base::FeatureParam<std::string> disable_patterns(
      &features::kVulkan, "disable_by_gl_renderer",
      "*Mali-G?? M*" /* https://crbug.com/1183702 */);
  if (MatchGLRenderer(gpu_info_, disable_patterns.Get()))
    return false;

  auto enable_patterns = base::GetFieldTrialParamValueByFeature(
      features::kVulkan, "force_enable_by_gl_renderer");
  forced_native |= MatchGLRenderer(gpu_info_, enable_patterns);

  if (!use_swiftshader && !forced_native &&
      !CheckVulkanCompabilities(
          vulkan_implementation_->GetVulkanInstance()->vulkan_info(), gpu_info_,
          base::GetFieldTrialParamValueByFeature(features::kVulkan,
                                                 "enable_by_device_name"))) {
    vulkan_implementation_.reset();
    return false;
  }

  gpu_info_.vulkan_info =
      vulkan_implementation_->GetVulkanInstance()->vulkan_info();
  return true;
}
#else   // BUILDFLAG(ENABLE_VULKAN)
bool GpuInit::InitializeVulkan() {
  return false;
}
#endif  // !BUILDFLAG(ENABLE_VULKAN)

}  // namespace gpu
