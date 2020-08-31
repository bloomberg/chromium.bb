// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_CONFIG_GPU_PREFERENCES_H_
#define GPU_CONFIG_GPU_PREFERENCES_H_

#include <stddef.h>
#include <string>
#include <vector>

#include "base/macros.h"
#include "build/build_config.h"
#include "gpu/gpu_export.h"
#include "media/media_buildflags.h"
#include "ui/gfx/buffer_types.h"

#if defined(USE_OZONE)
#include "base/message_loop/message_pump_type.h"
#endif

namespace gpu {

// The size to set for the program cache for default and low-end device cases.
#if !defined(OS_ANDROID)
const size_t kDefaultMaxProgramCacheMemoryBytes = 6 * 1024 * 1024;
#else
const size_t kDefaultMaxProgramCacheMemoryBytes = 2 * 1024 * 1024;
const size_t kLowEndMaxProgramCacheMemoryBytes = 128 * 1024;
#endif

enum class VulkanImplementationName : uint32_t {
  kNone = 0,
  kNative = 1,
  kForcedNative = 2,  // Cannot override by GPU blacklist.
  kSwiftshader = 3,
  kLast = kSwiftshader,
};

enum class GrContextType : uint32_t {
  kGL = 0,
  kVulkan = 1,
  kMetal = 2,
  kDawn = 3,
  kLast = kDawn,
};

// NOTE: if you modify this structure then you must also modify the
// following two files to keep them in sync:
//   src/gpu/ipc/common/gpu_preferences.mojom
//   src/gpu/ipc/common/gpu_preferences_mojom_traits.h
struct GPU_EXPORT GpuPreferences {
 public:
  GpuPreferences();

  GpuPreferences(const GpuPreferences& other);

  ~GpuPreferences();

  // Encode struct into a string so it can be passed as a commandline switch.
  std::string ToSwitchValue();

  // Decode the encoded string back to GpuPrefences struct.
  // If return false, |this| won't be touched.
  bool FromSwitchValue(const std::string& data);

  // ===================================
  // Settings from //content/public/common/content_switches.h

  // Disables hardware acceleration of video decode, where available.
  bool disable_accelerated_video_decode = false;

  // Disables hardware acceleration of video decode, where available.
  bool disable_accelerated_video_encode = false;

  // Causes the GPU process to display a dialog on launch.
  bool gpu_startup_dialog = false;

  // Disable the thread that crashes the GPU process if it stops responding to
  // messages.
  bool disable_gpu_watchdog = false;

  // Starts the GPU sandbox before creating a GL context.
  bool gpu_sandbox_start_early = false;

  // Enables using CODECAPI_AVLowLatencyMode. Windows only.
  bool enable_low_latency_dxva = true;

  // Enables support for avoiding copying DXGI NV12 textures. Windows only.
  bool enable_zero_copy_dxgi_video = false;

  // Enables support for outputting NV12 video frames. Windows only.
  bool enable_nv12_dxgi_video = false;

  // Enables MediaFoundationVideoEncoderAccelerator on Windows 7. Windows 7 does
  // not support some of the attributes which may impact the performance or the
  // quality of output. So this flag is disabled by default. Windows only.
  bool enable_media_foundation_vea_on_windows7 = false;

  // Disables the use of a 3D software rasterizer, for example, SwiftShader.
  bool disable_software_rasterizer = false;

  bool log_gpu_control_list_decisions = false;

  // ===================================
  // Settings from //gpu/command_buffer/service/gpu_switches.cc

  // Always return success when compiling a shader. Linking will still fail.
  bool compile_shader_always_succeeds = false;

  // Disable the GL error log limit.
  bool disable_gl_error_limit = false;

  // Disable the GLSL translator.
  bool disable_glsl_translator = false;

  // Turn off user-defined name hashing in shaders.
  bool disable_shader_name_hashing = false;

  // Turn on Logging GPU commands.
  bool enable_gpu_command_logging = false;

  // Turn on Calling GL Error after every command.
  bool enable_gpu_debugging = false;

  // Enable GPU service logging.
  bool enable_gpu_service_logging_gpu = false;

  // Enable logging of GPU driver debug messages.
  bool enable_gpu_driver_debug_logging = false;

  // Turn off gpu program caching
  bool disable_gpu_program_cache = false;

  // Enforce GL minimums.
  bool enforce_gl_minimums = false;

  // Sets the total amount of memory that may be allocated for GPU resources.
  uint32_t force_gpu_mem_available_bytes = 0u;

  // Sets the maximum discardable cache size limit for GPU resources.
  uint32_t force_gpu_mem_discardable_limit_bytes = 0u;

  // Sets maximum texture size.
  uint32_t force_max_texture_size = 0u;

  // Sets the maximum size of the in-memory gpu program cache, in kb
  uint32_t gpu_program_cache_size = kDefaultMaxProgramCacheMemoryBytes;

  // Disables the GPU shader on disk cache.
  bool disable_gpu_shader_disk_cache = false;

  // Simulates shared textures when share groups are not available.
  // Not available everywhere.
  bool enable_threaded_texture_mailboxes = false;

  // Include ANGLE's intermediate representation (AST) output in shader
  // compilation info logs.
  bool gl_shader_interm_output = false;

  // Emulate ESSL lowp and mediump float precisions by mutating the shaders to
  // round intermediate values in ANGLE.
  bool emulate_shader_precision = false;

  // ===================================
  // Settings from //gpu/config/gpu_switches.h

  // Enables the use of SurfaceControl for overlays on Android.
  bool enable_android_surface_control = false;

  // ===================================
  // Settings from //ui/gl/gl_switches.h

  // Turns on GPU logging (debug build only).
  bool enable_gpu_service_logging = false;

  // Turns on calling TRACE for every GL call.
  bool enable_gpu_service_tracing = false;

  // Use the Pass-through command decoder, skipping all validation and state
  // tracking.
  bool use_passthrough_cmd_decoder = false;

  // Disable using a single multiplanar GpuMemoryBuffer to store biplanar
  // VideoFrames (e.g. NV12), see https://crbug.com/791676.
  bool disable_biplanar_gpu_memory_buffers_for_video_frames = false;

  // List of texture usage & formats that require use of a platform specific
  // texture target.
  std::vector<gfx::BufferUsageAndFormat> texture_target_exception_list;

  // ===================================
  // Settings from //gpu/config/gpu_switches.h

  // Ignores GPU blacklist.
  bool ignore_gpu_blacklist = false;

  // Oop rasterization preferences in the GPU process.  disable wins over
  // enable, and neither means use defaults from GpuFeatureInfo.
  bool enable_oop_rasterization = false;
  bool disable_oop_rasterization = false;

  bool enable_oop_rasterization_ddl = false;

  // Start the watchdog suspended, as the app is already backgrounded and won't
  // send a background/suspend signal.
  bool watchdog_starts_backgrounded = false;

  // ===================================
  // Settings from //gpu/command_buffer/service/gpu_switches.h
  // The type of the GrContext.
  GrContextType gr_context_type = GrContextType::kGL;

  // Use Vulkan for rasterization and display compositing.
  VulkanImplementationName use_vulkan = VulkanImplementationName::kNone;

  // Enforce using vulkan protected memory.
  bool enforce_vulkan_protected_memory = false;

  // Use vulkan VK_KHR_surface for presenting.
  bool disable_vulkan_surface = false;

  // If Vulkan initialization has failed, do not fallback to GL. This is for
  // testing in order to detect regressions which crash Vulkan.
  bool disable_vulkan_fallback_to_gl_for_testing = false;

  // Use Metal for rasterization and Skia-based display compositing. Note that
  // this is compatible with GL-based display compositing.
  bool enable_metal = false;

  // ===================================
  // Settings from //cc/base/switches.h
  // Enable the GPU benchmarking extension; used by tests only.
  bool enable_gpu_benchmarking_extension = false;

  // Enable the WebGPU command buffer.
  bool enable_webgpu = false;

  // Enable validation layers in Dawn backends.
  bool enable_dawn_backend_validation = false;

  // Enable measuring blocked time on GPU Main thread
  bool enable_gpu_blocked_time_metric = false;

  // Enable collecting perf data for device categorization purpose. Currently
  // only enabled on Windows platform for the info collection GPU process.
  bool enable_perf_data_collection = false;

#if defined(USE_OZONE)
  // Determines message pump type for the GPU thread.
  base::MessagePumpType message_pump_type = base::MessagePumpType::DEFAULT;
#endif

  // ===================================
  // Settings from //ui/gfx/switches.h

  // Enable native CPU-mappable GPU memory buffer support on Linux.
  bool enable_native_gpu_memory_buffers = false;

  // ===================================
  // Settings from //media/base/media_switches.h

  // Force to disable new VideoDecoder.
  bool force_disable_new_accelerated_video_decoder = false;

  // Please update gpu_preferences_unittest.cc when making additions or
  // changes to this struct.
};

}  // namespace gpu

#endif  // GPU_CONFIG_GPU_PREFERENCES_H_
