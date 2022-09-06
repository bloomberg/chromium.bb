// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/gl_surface_egl.h"

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <memory>
#include <sstream>
#include <vector>

#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/debug/crash_logging.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_macros.h"
#include "base/scoped_environment_variable_override.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/system/sys_info.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "ui/events/platform/platform_event_dispatcher.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/gpu_fence.h"
#include "ui/gl/angle_platform_impl.h"
#include "ui/gl/egl_util.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_context_egl.h"
#include "ui/gl/gl_display_egl_util.h"
#include "ui/gl/gl_display_manager.h"
#include "ui/gl/gl_image.h"
#include "ui/gl/gl_implementation.h"
#include "ui/gl/gl_surface_presentation_helper.h"
#include "ui/gl/gl_surface_stub.h"
#include "ui/gl/gl_utils.h"
#include "ui/gl/gpu_switching_manager.h"
#include "ui/gl/scoped_make_current.h"
#include "ui/gl/sync_control_vsync_provider.h"

#if defined(USE_OZONE)
#include "ui/ozone/buildflags.h"
#endif  // defined(USE_OZONE)

#if BUILDFLAG(IS_ANDROID)
#include <android/native_window_jni.h>
#include "base/android/build_info.h"
#endif

#if !defined(EGL_FIXED_SIZE_ANGLE)
#define EGL_FIXED_SIZE_ANGLE 0x3201
#endif

#if !defined(EGL_OPENGL_ES3_BIT)
#define EGL_OPENGL_ES3_BIT 0x00000040
#endif

// Not present egl/eglext.h yet.

#ifndef EGL_EXT_gl_colorspace_display_p3
#define EGL_EXT_gl_colorspace_display_p3 1
#define EGL_GL_COLORSPACE_DISPLAY_P3_EXT 0x3363
#endif /* EGL_EXT_gl_colorspace_display_p3 */

#ifndef EGL_EXT_gl_colorspace_display_p3_passthrough
#define EGL_EXT_gl_colorspace_display_p3_passthrough 1
#define EGL_GL_COLORSPACE_DISPLAY_P3_PASSTHROUGH_EXT 0x3490
#endif /* EGL_EXT_gl_colorspace_display_p3_passthrough */

// From ANGLE's egl/eglext.h.

#ifndef EGL_ANGLE_platform_angle
#define EGL_ANGLE_platform_angle 1
#define EGL_PLATFORM_ANGLE_ANGLE 0x3202
#define EGL_PLATFORM_ANGLE_TYPE_ANGLE 0x3203
#define EGL_PLATFORM_ANGLE_MAX_VERSION_MAJOR_ANGLE 0x3204
#define EGL_PLATFORM_ANGLE_MAX_VERSION_MINOR_ANGLE 0x3205
#define EGL_PLATFORM_ANGLE_TYPE_DEFAULT_ANGLE 0x3206
#define EGL_PLATFORM_ANGLE_DEBUG_LAYERS_ENABLED_ANGLE 0x3451
#define EGL_PLATFORM_ANGLE_DEVICE_TYPE_ANGLE 0x3209
#define EGL_PLATFORM_ANGLE_DEVICE_TYPE_EGL_ANGLE 0x348E
#define EGL_PLATFORM_ANGLE_DEVICE_TYPE_HARDWARE_ANGLE 0x320A
#define EGL_PLATFORM_ANGLE_DEVICE_TYPE_NULL_ANGLE 0x345E
#define EGL_PLATFORM_ANGLE_DEVICE_TYPE_SWIFTSHADER_ANGLE 0x3487
#define EGL_PLATFORM_ANGLE_NATIVE_PLATFORM_TYPE_ANGLE 0x348F
#endif /* EGL_ANGLE_platform_angle */

#ifndef EGL_ANGLE_platform_angle_d3d
#define EGL_ANGLE_platform_angle_d3d 1
#define EGL_PLATFORM_ANGLE_TYPE_D3D9_ANGLE 0x3207
#define EGL_PLATFORM_ANGLE_TYPE_D3D11_ANGLE 0x3208
#define EGL_PLATFORM_ANGLE_DEVICE_TYPE_D3D_WARP_ANGLE 0x320B
#define EGL_PLATFORM_ANGLE_DEVICE_TYPE_D3D_REFERENCE_ANGLE 0x320C
#endif /* EGL_ANGLE_platform_angle_d3d */

#ifndef EGL_ANGLE_platform_angle_d3d_luid
#define EGL_ANGLE_platform_angle_d3d_luid 1
#define EGL_PLATFORM_ANGLE_D3D_LUID_HIGH_ANGLE 0x34A0
#define EGL_PLATFORM_ANGLE_D3D_LUID_LOW_ANGLE 0x34A1
#endif /* EGL_ANGLE_platform_angle_d3d_luid */

#ifndef EGL_ANGLE_platform_angle_d3d11on12
#define EGL_ANGLE_platform_angle_d3d11on12 1
#define EGL_PLATFORM_ANGLE_D3D11ON12_ANGLE 0x3488
#endif /* EGL_ANGLE_platform_angle_d3d11on12 */

#ifndef EGL_ANGLE_platform_angle_opengl
#define EGL_ANGLE_platform_angle_opengl 1
#define EGL_PLATFORM_ANGLE_TYPE_OPENGL_ANGLE 0x320D
#define EGL_PLATFORM_ANGLE_TYPE_OPENGLES_ANGLE 0x320E
#endif /* EGL_ANGLE_platform_angle_opengl */

#ifndef EGL_ANGLE_platform_angle_null
#define EGL_ANGLE_platform_angle_null 1
#define EGL_PLATFORM_ANGLE_TYPE_NULL_ANGLE 0x33AE
#endif /* EGL_ANGLE_platform_angle_null */

#ifndef EGL_ANGLE_platform_angle_vulkan
#define EGL_ANGLE_platform_angle_vulkan 1
#define EGL_PLATFORM_ANGLE_TYPE_VULKAN_ANGLE 0x3450
#define EGL_PLATFORM_VULKAN_DISPLAY_MODE_HEADLESS_ANGLE 0x34A5
#endif /* EGL_ANGLE_platform_angle_vulkan */

#ifndef EGL_ANGLE_robust_resource_initialization
#define EGL_ANGLE_robust_resource_initialization 1
#define EGL_ROBUST_RESOURCE_INITIALIZATION_ANGLE 0x3453
#endif /* EGL_ANGLE_display_robust_resource_initialization */

#ifndef EGL_ANGLE_platform_angle_metal
#define EGL_ANGLE_platform_angle_metal 1
#define EGL_PLATFORM_ANGLE_TYPE_METAL_ANGLE 0x3489
#endif /* EGL_ANGLE_platform_angle_metal */

#ifndef EGL_ANGLE_x11_visual
#define EGL_ANGLE_x11_visual 1
#define EGL_X11_VISUAL_ID_ANGLE 0x33A3
#endif /* EGL_ANGLE_x11_visual */

#ifndef EGL_ANGLE_surface_orientation
#define EGL_ANGLE_surface_orientation
#define EGL_OPTIMAL_SURFACE_ORIENTATION_ANGLE 0x33A7
#define EGL_SURFACE_ORIENTATION_ANGLE 0x33A8
#define EGL_SURFACE_ORIENTATION_INVERT_X_ANGLE 0x0001
#define EGL_SURFACE_ORIENTATION_INVERT_Y_ANGLE 0x0002
#endif /* EGL_ANGLE_surface_orientation */

#ifndef EGL_ANGLE_direct_composition
#define EGL_ANGLE_direct_composition 1
#define EGL_DIRECT_COMPOSITION_ANGLE 0x33A5
#endif /* EGL_ANGLE_direct_composition */

#ifndef EGL_ANGLE_display_robust_resource_initialization
#define EGL_ANGLE_display_robust_resource_initialization 1
#define EGL_DISPLAY_ROBUST_RESOURCE_INITIALIZATION_ANGLE 0x3453
#endif /* EGL_ANGLE_display_robust_resource_initialization */

#ifndef EGL_ANGLE_display_power_preference
#define EGL_ANGLE_display_power_preference 1
#define EGL_POWER_PREFERENCE_ANGLE 0x3482
#define EGL_LOW_POWER_ANGLE 0x0001
#define EGL_HIGH_POWER_ANGLE 0x0002
#endif /* EGL_ANGLE_power_preference */

#ifndef EGL_ANGLE_platform_angle_device_id
#define EGL_ANGLE_platform_angle_device_id
#define EGL_PLATFORM_ANGLE_DEVICE_ID_HIGH_ANGLE 0x34D6
#define EGL_PLATFORM_ANGLE_DEVICE_ID_LOW_ANGLE 0x34D7
#endif /* EGL_ANGLE_platform_angle_device_id */

// From ANGLE's egl/eglext.h.
#ifndef EGL_ANGLE_feature_control
#define EGL_ANGLE_feature_control 1
#define EGL_FEATURE_NAME_ANGLE 0x3460
#define EGL_FEATURE_CATEGORY_ANGLE 0x3461
#define EGL_FEATURE_DESCRIPTION_ANGLE 0x3462
#define EGL_FEATURE_BUG_ANGLE 0x3463
#define EGL_FEATURE_STATUS_ANGLE 0x3464
#define EGL_FEATURE_COUNT_ANGLE 0x3465
#define EGL_FEATURE_OVERRIDES_ENABLED_ANGLE 0x3466
#define EGL_FEATURE_OVERRIDES_DISABLED_ANGLE 0x3467
#define EGL_FEATURE_ALL_DISABLED_ANGLE 0x3469
#endif /* EGL_ANGLE_feature_control */

using ui::GetLastEGLErrorString;
using ui::PlatformEvent;

namespace gl {

namespace {

class EGLGpuSwitchingObserver;

EGLGpuSwitchingObserver* g_egl_gpu_switching_observer = nullptr;

constexpr const char kSwapEventTraceCategories[] = "gpu";

constexpr size_t kMaxTimestampsSupportable = 9;

struct TraceSwapEventsInitializer {
  TraceSwapEventsInitializer()
      : value(*TRACE_EVENT_API_GET_CATEGORY_GROUP_ENABLED(
            kSwapEventTraceCategories)) {}
  const unsigned char& value;
};

static base::LazyInstance<TraceSwapEventsInitializer>::Leaky
    g_trace_swap_enabled = LAZY_INSTANCE_INITIALIZER;

class EGLSyncControlVSyncProvider : public SyncControlVSyncProvider {
 public:
  EGLSyncControlVSyncProvider(EGLSurface surface, GLDisplayEGL* display)
      : surface_(surface), display_(display) {
    DCHECK(display_);
  }

  EGLSyncControlVSyncProvider(const EGLSyncControlVSyncProvider&) = delete;
  EGLSyncControlVSyncProvider& operator=(const EGLSyncControlVSyncProvider&) =
      delete;

  ~EGLSyncControlVSyncProvider() override {}

  static bool IsSupported(GLDisplayEGL* display) {
    DCHECK(display);
    return SyncControlVSyncProvider::IsSupported() &&
           display->egl_sync_control_supported;
  }

 protected:
  bool GetSyncValues(int64_t* system_time,
                     int64_t* media_stream_counter,
                     int64_t* swap_buffer_counter) override {
    uint64_t u_system_time, u_media_stream_counter, u_swap_buffer_counter;
    bool result =
        eglGetSyncValuesCHROMIUM(display_->GetDisplay(), surface_,
                                 &u_system_time, &u_media_stream_counter,
                                 &u_swap_buffer_counter) == EGL_TRUE;
    if (result) {
      *system_time = static_cast<int64_t>(u_system_time);
      *media_stream_counter = static_cast<int64_t>(u_media_stream_counter);
      *swap_buffer_counter = static_cast<int64_t>(u_swap_buffer_counter);
    }
    return result;
  }

  bool GetMscRate(int32_t* numerator, int32_t* denominator) override {
    if (!display_->egl_sync_control_rate_supported) {
      return false;
    }

    bool result = eglGetMscRateANGLE(display_->GetDisplay(), surface_,
                                     numerator, denominator) == EGL_TRUE;
    return result;
  }

  bool IsHWClock() const override { return true; }

 private:
  EGLSurface surface_;
  raw_ptr<GLDisplayEGL> display_;
};

class EGLGpuSwitchingObserver final : public ui::GpuSwitchingObserver {
 public:
  explicit EGLGpuSwitchingObserver(GLDisplayEGL* display) : display_(display) {
    DCHECK(display_);
  }

  void OnGpuSwitched(gl::GpuPreference active_gpu_heuristic) override {
    DCHECK(display_->IsANGLEPowerPreferenceSupported());
    eglHandleGPUSwitchANGLE(display_->GetDisplay());
  }

 private:
  raw_ptr<GLDisplayEGL> display_ = nullptr;
};

std::vector<const char*> GetAttribArrayFromStringVector(
    const std::vector<std::string>& strings) {
  std::vector<const char*> attribs;
  for (const std::string& item : strings) {
    attribs.push_back(item.c_str());
  }
  attribs.push_back(0);
  return attribs;
}

std::vector<std::string> GetStringVectorFromCommandLine(
    const base::CommandLine* command_line,
    const char switch_name[]) {
  std::string command_string = command_line->GetSwitchValueASCII(switch_name);
  return base::SplitString(command_string, ", ;", base::TRIM_WHITESPACE,
                           base::SPLIT_WANT_NONEMPTY);
}

EGLDisplay GetPlatformANGLEDisplay(
    GLDisplayEGL* gl_display,
    EGLenum platform_type,
    const std::vector<std::string>& enabled_features,
    const std::vector<std::string>& disabled_features,
    const std::vector<EGLAttrib>& extra_display_attribs) {
  std::vector<EGLAttrib> display_attribs(extra_display_attribs);

  display_attribs.push_back(EGL_PLATFORM_ANGLE_TYPE_ANGLE);
  display_attribs.push_back(static_cast<EGLAttrib>(platform_type));

  if (platform_type == EGL_PLATFORM_ANGLE_TYPE_D3D11_ANGLE) {
    base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
    if (command_line->HasSwitch(switches::kUseAdapterLuid)) {
      // If the LUID is specified, the format is <high part>,<low part>. Split
      // and add them to the EGL_ANGLE_platform_angle_d3d_luid ext attributes.
      std::string luid =
          command_line->GetSwitchValueASCII(switches::kUseAdapterLuid);
      size_t comma = luid.find(',');
      if (comma != std::string::npos) {
        int32_t high;
        uint32_t low;
        if (!base::StringToInt(luid.substr(0, comma), &high) ||
            !base::StringToUint(luid.substr(comma + 1), &low))
          return EGL_NO_DISPLAY;

        display_attribs.push_back(EGL_PLATFORM_ANGLE_D3D_LUID_HIGH_ANGLE);
        display_attribs.push_back(high);

        display_attribs.push_back(EGL_PLATFORM_ANGLE_D3D_LUID_LOW_ANGLE);
        display_attribs.push_back(low);
      }
    }
  }

  GLDisplayEglUtil::GetInstance()->GetPlatformExtraDisplayAttribs(
      platform_type, &display_attribs);

  std::vector<const char*> enabled_features_attribs =
      GetAttribArrayFromStringVector(enabled_features);
  std::vector<const char*> disabled_features_attribs =
      GetAttribArrayFromStringVector(disabled_features);
  if (gl_display->egl_angle_feature_control_supported) {
    if (!enabled_features_attribs.empty()) {
      display_attribs.push_back(EGL_FEATURE_OVERRIDES_ENABLED_ANGLE);
      display_attribs.push_back(
          reinterpret_cast<EGLAttrib>(enabled_features_attribs.data()));
    }
    if (!disabled_features_attribs.empty()) {
      display_attribs.push_back(EGL_FEATURE_OVERRIDES_DISABLED_ANGLE);
      display_attribs.push_back(
          reinterpret_cast<EGLAttrib>(disabled_features_attribs.data()));
    }
  }
  // TODO(dbehr) Add an attrib to Angle to pass EGL platform.

  if (gl_display->IsANGLEDisplayPowerPreferenceSupported()) {
    GpuPreference pref =
        GLSurface::AdjustGpuPreference(GpuPreference::kDefault);
    switch (pref) {
      case GpuPreference::kDefault:
        // Don't request any GPU, let ANGLE and the native driver decide.
        break;
      case GpuPreference::kLowPower:
        display_attribs.push_back(EGL_POWER_PREFERENCE_ANGLE);
        display_attribs.push_back(EGL_LOW_POWER_ANGLE);
        break;
      case GpuPreference::kHighPerformance:
        display_attribs.push_back(EGL_POWER_PREFERENCE_ANGLE);
        display_attribs.push_back(EGL_HIGH_POWER_ANGLE);
        break;
      default:
        NOTREACHED();
    }
  }

  display_attribs.push_back(EGL_NONE);

  // This is an EGL 1.5 function that we know ANGLE supports. It's used to pass
  // EGLAttribs (pointers) instead of EGLints into the display
  return eglGetPlatformDisplay(
      EGL_PLATFORM_ANGLE_ANGLE,
      reinterpret_cast<void*>(gl_display->GetNativeDisplay()),
      &display_attribs[0]);
}

EGLDisplay GetDisplayFromType(
    DisplayType display_type,
    GLDisplayEGL* gl_display,
    const std::vector<std::string>& enabled_angle_features,
    const std::vector<std::string>& disabled_angle_features,
    bool disable_all_angle_features,
    uint64_t system_device_id) {
  DCHECK(gl_display);
  std::vector<EGLAttrib> extra_display_attribs;
  if (disable_all_angle_features) {
    extra_display_attribs.push_back(EGL_FEATURE_ALL_DISABLED_ANGLE);
    extra_display_attribs.push_back(EGL_TRUE);
  }
  if (system_device_id != 0 &&
      gl_display->IsANGLEPlatformANGLEDeviceIdSupported()) {
    uint32_t low_part = system_device_id & 0xffffffff;
    extra_display_attribs.push_back(EGL_PLATFORM_ANGLE_DEVICE_ID_LOW_ANGLE);
    extra_display_attribs.push_back(low_part);

    uint32_t high_part = (system_device_id >> 32) & 0xffffffff;
    extra_display_attribs.push_back(EGL_PLATFORM_ANGLE_DEVICE_ID_HIGH_ANGLE);
    extra_display_attribs.push_back(high_part);
  }
  switch (display_type) {
    case DEFAULT:
    case SWIFT_SHADER: {
      EGLDisplayPlatform native_display = gl_display->native_display;
      if (native_display.GetPlatform() != 0) {
        return eglGetPlatformDisplay(
            native_display.GetPlatform(),
            reinterpret_cast<void*>(native_display.GetDisplay()), nullptr);
      } else {
        return eglGetDisplay(native_display.GetDisplay());
      }
    }
    case ANGLE_D3D9:
      return GetPlatformANGLEDisplay(
          gl_display, EGL_PLATFORM_ANGLE_TYPE_D3D9_ANGLE,
          enabled_angle_features, disabled_angle_features,
          extra_display_attribs);
    case ANGLE_D3D11:
      return GetPlatformANGLEDisplay(
          gl_display, EGL_PLATFORM_ANGLE_TYPE_D3D11_ANGLE,
          enabled_angle_features, disabled_angle_features,
          extra_display_attribs);
    case ANGLE_D3D11_NULL:
      extra_display_attribs.push_back(EGL_PLATFORM_ANGLE_DEVICE_TYPE_ANGLE);
      extra_display_attribs.push_back(
          EGL_PLATFORM_ANGLE_DEVICE_TYPE_NULL_ANGLE);
      return GetPlatformANGLEDisplay(
          gl_display, EGL_PLATFORM_ANGLE_TYPE_D3D11_ANGLE,
          enabled_angle_features, disabled_angle_features,
          extra_display_attribs);
    case ANGLE_OPENGL:
      return GetPlatformANGLEDisplay(
          gl_display, EGL_PLATFORM_ANGLE_TYPE_OPENGL_ANGLE,
          enabled_angle_features, disabled_angle_features,
          extra_display_attribs);
    case ANGLE_OPENGL_EGL:
      extra_display_attribs.push_back(EGL_PLATFORM_ANGLE_DEVICE_TYPE_ANGLE);
      extra_display_attribs.push_back(EGL_PLATFORM_ANGLE_DEVICE_TYPE_EGL_ANGLE);
      return GetPlatformANGLEDisplay(
          gl_display, EGL_PLATFORM_ANGLE_TYPE_OPENGL_ANGLE,
          enabled_angle_features, disabled_angle_features,
          extra_display_attribs);
    case ANGLE_OPENGL_NULL:
      extra_display_attribs.push_back(EGL_PLATFORM_ANGLE_DEVICE_TYPE_ANGLE);
      extra_display_attribs.push_back(
          EGL_PLATFORM_ANGLE_DEVICE_TYPE_NULL_ANGLE);
      return GetPlatformANGLEDisplay(
          gl_display, EGL_PLATFORM_ANGLE_TYPE_OPENGL_ANGLE,
          enabled_angle_features, disabled_angle_features,
          extra_display_attribs);
    case ANGLE_OPENGLES:
      return GetPlatformANGLEDisplay(
          gl_display, EGL_PLATFORM_ANGLE_TYPE_OPENGLES_ANGLE,
          enabled_angle_features, disabled_angle_features,
          extra_display_attribs);
    case ANGLE_OPENGLES_EGL:
      extra_display_attribs.push_back(EGL_PLATFORM_ANGLE_DEVICE_TYPE_ANGLE);
      extra_display_attribs.push_back(EGL_PLATFORM_ANGLE_DEVICE_TYPE_EGL_ANGLE);
      return GetPlatformANGLEDisplay(
          gl_display, EGL_PLATFORM_ANGLE_TYPE_OPENGLES_ANGLE,
          enabled_angle_features, disabled_angle_features,
          extra_display_attribs);
    case ANGLE_OPENGLES_NULL:
      extra_display_attribs.push_back(EGL_PLATFORM_ANGLE_DEVICE_TYPE_ANGLE);
      extra_display_attribs.push_back(
          EGL_PLATFORM_ANGLE_DEVICE_TYPE_NULL_ANGLE);
      return GetPlatformANGLEDisplay(
          gl_display, EGL_PLATFORM_ANGLE_TYPE_OPENGLES_ANGLE,
          enabled_angle_features, disabled_angle_features,
          extra_display_attribs);
    case ANGLE_NULL:
      return GetPlatformANGLEDisplay(
          gl_display, EGL_PLATFORM_ANGLE_TYPE_NULL_ANGLE,
          enabled_angle_features, disabled_angle_features,
          extra_display_attribs);
    case ANGLE_VULKAN:
      return GetPlatformANGLEDisplay(
          gl_display, EGL_PLATFORM_ANGLE_TYPE_VULKAN_ANGLE,
          enabled_angle_features, disabled_angle_features,
          extra_display_attribs);
    case ANGLE_VULKAN_NULL:
      extra_display_attribs.push_back(EGL_PLATFORM_ANGLE_DEVICE_TYPE_ANGLE);
      extra_display_attribs.push_back(
          EGL_PLATFORM_ANGLE_DEVICE_TYPE_NULL_ANGLE);
      return GetPlatformANGLEDisplay(
          gl_display, EGL_PLATFORM_ANGLE_TYPE_VULKAN_ANGLE,
          enabled_angle_features, disabled_angle_features,
          extra_display_attribs);
    case ANGLE_D3D11on12:
      extra_display_attribs.push_back(EGL_PLATFORM_ANGLE_D3D11ON12_ANGLE);
      extra_display_attribs.push_back(EGL_TRUE);
      return GetPlatformANGLEDisplay(
          gl_display, EGL_PLATFORM_ANGLE_TYPE_D3D11_ANGLE,
          enabled_angle_features, disabled_angle_features,
          extra_display_attribs);
    case ANGLE_SWIFTSHADER:
      extra_display_attribs.push_back(EGL_PLATFORM_ANGLE_DEVICE_TYPE_ANGLE);
      extra_display_attribs.push_back(
          EGL_PLATFORM_ANGLE_DEVICE_TYPE_SWIFTSHADER_ANGLE);
#if defined(USE_OZONE)
#if BUILDFLAG(OZONE_PLATFORM_X11)
      extra_display_attribs.push_back(EGL_PLATFORM_ANGLE_NATIVE_PLATFORM_TYPE_ANGLE);
      extra_display_attribs.push_back(EGL_PLATFORM_VULKAN_DISPLAY_MODE_HEADLESS_ANGLE);
#endif  // BUILDFLAG(OZONE_PLATFORM_X11)
#endif  // defined(USE_OZONE)
      return GetPlatformANGLEDisplay(
          gl_display, EGL_PLATFORM_ANGLE_TYPE_VULKAN_ANGLE,
          enabled_angle_features, disabled_angle_features,
          extra_display_attribs);
    case ANGLE_METAL:
      return GetPlatformANGLEDisplay(
          gl_display, EGL_PLATFORM_ANGLE_TYPE_METAL_ANGLE,
          enabled_angle_features, disabled_angle_features,
          extra_display_attribs);
    case ANGLE_METAL_NULL:
      extra_display_attribs.push_back(EGL_PLATFORM_ANGLE_DEVICE_TYPE_ANGLE);
      extra_display_attribs.push_back(
          EGL_PLATFORM_ANGLE_DEVICE_TYPE_NULL_ANGLE);
      return GetPlatformANGLEDisplay(
          gl_display, EGL_PLATFORM_ANGLE_TYPE_METAL_ANGLE,
          enabled_angle_features, disabled_angle_features,
          extra_display_attribs);
    default:
      NOTREACHED();
      return EGL_NO_DISPLAY;
  }
}

ANGLEImplementation GetANGLEImplementationFromDisplayType(
    DisplayType display_type) {
  switch (display_type) {
    case ANGLE_D3D9:
      return ANGLEImplementation::kD3D9;
    case ANGLE_D3D11:
    case ANGLE_D3D11_NULL:
    case ANGLE_D3D11on12:
      return ANGLEImplementation::kD3D11;
    case ANGLE_OPENGL:
    case ANGLE_OPENGL_NULL:
      return ANGLEImplementation::kOpenGL;
    case ANGLE_OPENGLES:
    case ANGLE_OPENGLES_NULL:
      return ANGLEImplementation::kOpenGLES;
    case ANGLE_NULL:
      return ANGLEImplementation::kNull;
    case ANGLE_VULKAN:
    case ANGLE_VULKAN_NULL:
      return ANGLEImplementation::kVulkan;
    case ANGLE_SWIFTSHADER:
      return ANGLEImplementation::kSwiftShader;
    case ANGLE_METAL:
    case ANGLE_METAL_NULL:
      return ANGLEImplementation::kMetal;
    default:
      return ANGLEImplementation::kNone;
  }
}

const char* DisplayTypeString(DisplayType display_type) {
  switch (display_type) {
    case DEFAULT:
      return "Default";
    case SWIFT_SHADER:
      return "SwiftShader";
    case ANGLE_D3D9:
      return "D3D9";
    case ANGLE_D3D11:
      return "D3D11";
    case ANGLE_D3D11_NULL:
      return "D3D11Null";
    case ANGLE_OPENGL:
      return "OpenGL";
    case ANGLE_OPENGL_NULL:
      return "OpenGLNull";
    case ANGLE_OPENGLES:
      return "OpenGLES";
    case ANGLE_OPENGLES_NULL:
      return "OpenGLESNull";
    case ANGLE_NULL:
      return "Null";
    case ANGLE_VULKAN:
      return "Vulkan";
    case ANGLE_VULKAN_NULL:
      return "VulkanNull";
    case ANGLE_D3D11on12:
      return "D3D11on12";
    case ANGLE_SWIFTSHADER:
      return "SwANGLE";
    case ANGLE_OPENGL_EGL:
      return "OpenGLEGL";
    case ANGLE_OPENGLES_EGL:
      return "OpenGLESEGL";
    case ANGLE_METAL:
      return "Metal";
    case ANGLE_METAL_NULL:
      return "MetalNull";
    default:
      NOTREACHED();
      return "Err";
  }
}

bool ValidateEglConfig(EGLDisplay display,
                       const EGLint* config_attribs,
                       EGLint* num_configs) {
  if (!eglChooseConfig(display,
                       config_attribs,
                       NULL,
                       0,
                       num_configs)) {
    LOG(ERROR) << "eglChooseConfig failed with error "
               << GetLastEGLErrorString();
    return false;
  }
  if (*num_configs == 0) {
    return false;
  }
  return true;
}

EGLConfig ChooseConfig(EGLDisplay display,
                       GLSurfaceFormat format,
                       bool surfaceless,
                       bool offscreen,
                       EGLint visual_id) {
  // Choose an EGL configuration.
  // On X this is only used for PBuffer surfaces.

  std::vector<EGLint> renderable_types;
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableES3GLContext)) {
    renderable_types.push_back(EGL_OPENGL_ES3_BIT);
  }
  renderable_types.push_back(EGL_OPENGL_ES2_BIT);

  EGLint buffer_size = format.GetBufferSize();
  EGLint alpha_size = 8;
  bool want_rgb565 = buffer_size == 16;
  EGLint depth_size = format.GetDepthBits();
  EGLint stencil_size = format.GetStencilBits();
  EGLint samples = format.GetSamples();

  // Some platforms (eg. X11) may want to set custom values for alpha and buffer
  // sizes.
  GLDisplayEglUtil::GetInstance()->ChoosePlatformCustomAlphaAndBufferSize(
      &alpha_size, &buffer_size);

  EGLint surface_type =
      (surfaceless
           ? EGL_DONT_CARE
           : (offscreen ? EGL_PBUFFER_BIT : EGL_WINDOW_BIT | EGL_PBUFFER_BIT));

  for (auto renderable_type : renderable_types) {
    EGLint config_attribs_8888[] = {EGL_BUFFER_SIZE,
                                    buffer_size,
                                    EGL_ALPHA_SIZE,
                                    alpha_size,
                                    EGL_BLUE_SIZE,
                                    8,
                                    EGL_GREEN_SIZE,
                                    8,
                                    EGL_RED_SIZE,
                                    8,
                                    EGL_SAMPLES,
                                    samples,
                                    EGL_DEPTH_SIZE,
                                    depth_size,
                                    EGL_STENCIL_SIZE,
                                    stencil_size,
                                    EGL_RENDERABLE_TYPE,
                                    renderable_type,
                                    EGL_SURFACE_TYPE,
                                    surface_type,
                                    EGL_NONE};

    EGLint config_attribs_565[] = {EGL_BUFFER_SIZE,
                                   16,
                                   EGL_BLUE_SIZE,
                                   5,
                                   EGL_GREEN_SIZE,
                                   6,
                                   EGL_RED_SIZE,
                                   5,
                                   EGL_SAMPLES,
                                   samples,
                                   EGL_DEPTH_SIZE,
                                   depth_size,
                                   EGL_STENCIL_SIZE,
                                   stencil_size,
                                   EGL_RENDERABLE_TYPE,
                                   renderable_type,
                                   EGL_SURFACE_TYPE,
                                   surface_type,
                                   EGL_NONE};

    EGLint* choose_attributes = config_attribs_8888;
    if (want_rgb565) {
      choose_attributes = config_attribs_565;
    }

    EGLint num_configs;
    EGLint config_size = 1;
    EGLConfig config = nullptr;
    EGLConfig* config_data = &config;
    // Validate if there are any configs for given attribs.
    if (!ValidateEglConfig(display, choose_attributes, &num_configs)) {
      // Try the next renderable_type
      continue;
    }

    std::unique_ptr<EGLConfig[]> matching_configs(new EGLConfig[num_configs]);
    if (want_rgb565 || visual_id >= 0) {
      config_size = num_configs;
      config_data = matching_configs.get();
    }

    if (!eglChooseConfig(display, choose_attributes, config_data, config_size,
                         &num_configs)) {
      LOG(ERROR) << "eglChooseConfig failed with error "
                 << GetLastEGLErrorString();
      return config;
    }

    if (want_rgb565) {
      // Because of the EGL config sort order, we have to iterate
      // through all of them (it'll put higher sum(R,G,B) bits
      // first with the above attribs).
      bool match_found = false;
      for (int i = 0; i < num_configs; i++) {
        EGLint red, green, blue, alpha;
        // Read the relevant attributes of the EGLConfig.
        if (eglGetConfigAttrib(display, matching_configs[i], EGL_RED_SIZE,
                               &red) &&
            eglGetConfigAttrib(display, matching_configs[i], EGL_BLUE_SIZE,
                               &blue) &&
            eglGetConfigAttrib(display, matching_configs[i], EGL_GREEN_SIZE,
                               &green) &&
            eglGetConfigAttrib(display, matching_configs[i], EGL_ALPHA_SIZE,
                               &alpha) &&
            alpha == 0 && red == 5 && green == 6 && blue == 5) {
          config = matching_configs[i];
          match_found = true;
          break;
        }
      }
      if (!match_found) {
        // To fall back to default 32 bit format, choose with
        // the right attributes again.
        if (!ValidateEglConfig(display, config_attribs_8888, &num_configs)) {
          // Try the next renderable_type
          continue;
        }
        if (!eglChooseConfig(display, config_attribs_8888, &config, 1,
                             &num_configs)) {
          LOG(ERROR) << "eglChooseConfig failed with error "
                     << GetLastEGLErrorString();
          return config;
        }
      }
    } else if (visual_id >= 0) {
      for (int i = 0; i < num_configs; i++) {
        EGLint id;
        if (eglGetConfigAttrib(display, matching_configs[i],
                               EGL_NATIVE_VISUAL_ID, &id) &&
            id == visual_id) {
          config = matching_configs[i];
          break;
        }
      }
    }
    return config;
  }

  LOG(ERROR) << "No suitable EGL configs found.";
  return nullptr;
}

void AddInitDisplay(std::vector<DisplayType>* init_displays,
                    DisplayType display_type) {
  // Make sure to not add the same display type twice.
  if (!base::Contains(*init_displays, display_type))
    init_displays->push_back(display_type);
}

const char* GetDebugMessageTypeString(EGLint source) {
  switch (source) {
    case EGL_DEBUG_MSG_CRITICAL_KHR:
      return "Critical";
    case EGL_DEBUG_MSG_ERROR_KHR:
      return "Error";
    case EGL_DEBUG_MSG_WARN_KHR:
      return "Warning";
    case EGL_DEBUG_MSG_INFO_KHR:
      return "Info";
    default:
      return "UNKNOWN";
  }
}

static void EGLAPIENTRY LogEGLDebugMessage(EGLenum error,
                                           const char* command,
                                           EGLint message_type,
                                           EGLLabelKHR thread_label,
                                           EGLLabelKHR object_label,
                                           const char* message) {
  std::string formatted_message = std::string("EGL Driver message (") +
                                  GetDebugMessageTypeString(message_type) +
                                  ") " + command + ": " + message;

  // Assume that all labels that have been set are strings
  if (thread_label) {
    formatted_message += " thread: ";
    formatted_message += static_cast<const char*>(thread_label);
  }
  if (object_label) {
    formatted_message += " object: ";
    formatted_message += static_cast<const char*>(object_label);
  }

  if (message_type == EGL_DEBUG_MSG_CRITICAL_KHR ||
      message_type == EGL_DEBUG_MSG_ERROR_KHR) {
    LOG(ERROR) << formatted_message;
  } else {
    DVLOG(1) << formatted_message;
  }
}

}  // namespace

void GetEGLInitDisplays(bool supports_angle_d3d,
                        bool supports_angle_opengl,
                        bool supports_angle_null,
                        bool supports_angle_vulkan,
                        bool supports_angle_swiftshader,
                        bool supports_angle_egl,
                        bool supports_angle_metal,
                        const base::CommandLine* command_line,
                        std::vector<DisplayType>* init_displays) {
  // If we're already requesting software GL, make sure we don't fallback to the
  // GPU
  bool forceSoftwareGL = IsSoftwareGLImplementation(GetGLImplementationParts());

  std::string requested_renderer =
      forceSoftwareGL ? kANGLEImplementationSwiftShaderName
                      : command_line->GetSwitchValueASCII(switches::kUseANGLE);

  bool use_angle_default =
      !forceSoftwareGL &&
      (!command_line->HasSwitch(switches::kUseANGLE) ||
       requested_renderer == kANGLEImplementationDefaultName);

  if (supports_angle_null &&
      requested_renderer == kANGLEImplementationNullName) {
    AddInitDisplay(init_displays, ANGLE_NULL);
    return;
  }

  // If no display has been explicitly requested and the DefaultANGLEOpenGL
  // experiment is enabled, try creating OpenGL displays first.
  // TODO(oetuaho@nvidia.com): Only enable this path on specific GPUs with a
  // blocklist entry. http://crbug.com/693090
  if (supports_angle_opengl && use_angle_default &&
      base::FeatureList::IsEnabled(features::kDefaultANGLEOpenGL)) {
    AddInitDisplay(init_displays, ANGLE_OPENGL);
    AddInitDisplay(init_displays, ANGLE_OPENGLES);
  }

  if (supports_angle_metal && use_angle_default &&
      base::FeatureList::IsEnabled(features::kDefaultANGLEMetal)) {
    AddInitDisplay(init_displays, ANGLE_METAL);
  }

  if (supports_angle_vulkan && use_angle_default &&
      features::IsDefaultANGLEVulkan()) {
    AddInitDisplay(init_displays, ANGLE_VULKAN);
  }

  if (supports_angle_d3d) {
    if (use_angle_default) {
      // Default mode for ANGLE - try D3D11, else try D3D9
      if (!command_line->HasSwitch(switches::kDisableD3D11)) {
        AddInitDisplay(init_displays, ANGLE_D3D11);
      }
      AddInitDisplay(init_displays, ANGLE_D3D9);
    } else {
      if (requested_renderer == kANGLEImplementationD3D11Name) {
        AddInitDisplay(init_displays, ANGLE_D3D11);
      } else if (requested_renderer == kANGLEImplementationD3D9Name) {
        AddInitDisplay(init_displays, ANGLE_D3D9);
      } else if (requested_renderer == kANGLEImplementationD3D11NULLName) {
        AddInitDisplay(init_displays, ANGLE_D3D11_NULL);
      } else if (requested_renderer == kANGLEImplementationD3D11on12Name) {
        AddInitDisplay(init_displays, ANGLE_D3D11on12);
      }
    }
  }

  if (supports_angle_opengl) {
    if (use_angle_default && !supports_angle_d3d) {
#if BUILDFLAG(IS_ANDROID)
      // Don't request desktopGL on android
      AddInitDisplay(init_displays, ANGLE_OPENGLES);
#else
      AddInitDisplay(init_displays, ANGLE_OPENGL);
      AddInitDisplay(init_displays, ANGLE_OPENGLES);
#endif
    } else {
      if (requested_renderer == kANGLEImplementationOpenGLName) {
        AddInitDisplay(init_displays, ANGLE_OPENGL);
      } else if (requested_renderer == kANGLEImplementationOpenGLESName) {
        AddInitDisplay(init_displays, ANGLE_OPENGLES);
      } else if (requested_renderer == kANGLEImplementationOpenGLNULLName) {
        AddInitDisplay(init_displays, ANGLE_OPENGL_NULL);
      } else if (requested_renderer == kANGLEImplementationOpenGLESNULLName) {
        AddInitDisplay(init_displays, ANGLE_OPENGLES_NULL);
      } else if (requested_renderer == kANGLEImplementationOpenGLEGLName &&
                 supports_angle_egl) {
        AddInitDisplay(init_displays, ANGLE_OPENGL_EGL);
      } else if (requested_renderer == kANGLEImplementationOpenGLESEGLName &&
                 supports_angle_egl) {
        AddInitDisplay(init_displays, ANGLE_OPENGLES_EGL);
      }
    }
  }

  if (supports_angle_vulkan) {
    if (use_angle_default) {
      if (!supports_angle_d3d && !supports_angle_opengl) {
        AddInitDisplay(init_displays, ANGLE_VULKAN);
      }
    } else if (requested_renderer == kANGLEImplementationVulkanName) {
      AddInitDisplay(init_displays, ANGLE_VULKAN);
    } else if (requested_renderer == kANGLEImplementationVulkanNULLName) {
      AddInitDisplay(init_displays, ANGLE_VULKAN_NULL);
    }
  }

  if (supports_angle_swiftshader) {
    if (requested_renderer == kANGLEImplementationSwiftShaderName ||
        requested_renderer == kANGLEImplementationSwiftShaderForWebGLName) {
      AddInitDisplay(init_displays, ANGLE_SWIFTSHADER);
    }
  }

  if (supports_angle_metal) {
    if (use_angle_default) {
      if (!supports_angle_opengl) {
        AddInitDisplay(init_displays, ANGLE_METAL);
      }
    } else if (requested_renderer == kANGLEImplementationMetalName) {
      AddInitDisplay(init_displays, ANGLE_METAL);
    } else if (requested_renderer == kANGLEImplementationMetalNULLName) {
      AddInitDisplay(init_displays, ANGLE_METAL_NULL);
    }
  }

  // If no displays are available due to missing angle extensions or invalid
  // flags, request the default display.
  if (init_displays->empty()) {
    init_displays->push_back(DEFAULT);
  }
}

GLSurfaceEGL::GLSurfaceEGL(GLDisplayEGL* display) : display_(display) {
  DCHECK(display_);
}

GLSurfaceFormat GLSurfaceEGL::GetFormat() {
  return format_;
}

GLDisplay* GLSurfaceEGL::GetGLDisplay() {
  return display_;
}

EGLConfig GLSurfaceEGL::GetConfig() {
  if (!config_) {
    config_ = ChooseConfig(display_->GetDisplay(), format_, IsSurfaceless(),
                           IsOffscreen(), GetNativeVisualID());
  }
  return config_;
}

EGLint GLSurfaceEGL::GetNativeVisualID() const {
  return -1;
}

EGLDisplay GLSurfaceEGL::GetEGLDisplay() {
  return display_->GetDisplay();
}

// static
GLDisplayEGL* GLSurfaceEGL::GetGLDisplayEGL() {
  return GLDisplayManagerEGL::GetInstance()->GetDisplay(
      GpuPreference::kDefault);
}

// static
GLDisplayEGL* GLSurfaceEGL::InitializeOneOff(EGLDisplayPlatform native_display,
                                             uint64_t system_device_id) {
  GLDisplayEGL* display =
      GLDisplayManagerEGL::GetInstance()->GetDisplay(system_device_id);
  if (display->GetDisplay() == EGL_NO_DISPLAY) {
    // Must be called before InitializeDisplay().
    g_driver_egl.ext.InitializeClientExtensionSettings();

    display = InitializeDisplay(native_display, system_device_id);
    if (display->GetDisplay() == EGL_NO_DISPLAY)
      return nullptr;

    // Must be called after InitializeDisplay().
    g_driver_egl.ext.InitializeExtensionSettings(display);

    InitializeOneOffCommon(display);
  }
  return display;
}

// static
GLDisplayEGL* GLSurfaceEGL::InitializeOneOffForTesting() {
  g_driver_egl.ext.InitializeClientExtensionSettings();
  GLDisplayEGL* display =
      GLDisplayManagerEGL::GetInstance()->GetDisplay(GpuPreference::kDefault);
  display->SetDisplay(eglGetCurrentDisplay());
  g_driver_egl.ext.InitializeExtensionSettings(display);
  InitializeOneOffCommon(display);
  return display;
}

// static
void GLSurfaceEGL::InitializeOneOffCommon(GLDisplayEGL* display) {
  display->egl_client_extensions =
      eglQueryString(EGL_NO_DISPLAY, EGL_EXTENSIONS);
  display->egl_extensions =
      eglQueryString(display->GetDisplay(), EGL_EXTENSIONS);

  display->egl_create_context_robustness_supported =
      display->HasEGLExtension("EGL_EXT_create_context_robustness");
  display->egl_robustness_video_memory_purge_supported =
      display->HasEGLExtension("EGL_NV_robustness_video_memory_purge");
  display->egl_create_context_bind_generates_resource_supported =
      display->HasEGLExtension(
          "EGL_CHROMIUM_create_context_bind_generates_resource");
  display->egl_create_context_webgl_compatability_supported =
      display->HasEGLExtension("EGL_ANGLE_create_context_webgl_compatibility");
  display->egl_sync_control_supported =
      display->HasEGLExtension("EGL_CHROMIUM_sync_control");
  display->egl_sync_control_rate_supported =
      display->HasEGLExtension("EGL_ANGLE_sync_control_rate");
  display->egl_window_fixed_size_supported =
      display->HasEGLExtension("EGL_ANGLE_window_fixed_size");
  display->egl_surface_orientation_supported =
      display->HasEGLExtension("EGL_ANGLE_surface_orientation");
  display->egl_khr_colorspace =
      display->HasEGLExtension("EGL_KHR_gl_colorspace");
  display->egl_ext_colorspace_display_p3 =
      display->HasEGLExtension("EGL_EXT_gl_colorspace_display_p3");
  display->egl_ext_colorspace_display_p3_passthrough =
      display->HasEGLExtension("EGL_EXT_gl_colorspace_display_p3_passthrough");
  // According to https://source.android.com/compatibility/android-cdd.html the
  // EGL_IMG_context_priority extension is mandatory for Virtual Reality High
  // Performance support, but due to a bug in Android Nougat the extension
  // isn't being reported even when it's present. As a fallback, check if other
  // related extensions that were added for VR support are present, and assume
  // that this implies context priority is also supported. See also:
  // https://github.com/googlevr/gvr-android-sdk/issues/330
  display->egl_context_priority_supported =
      display->HasEGLExtension("EGL_IMG_context_priority") ||
      (display->HasEGLExtension("EGL_ANDROID_front_buffer_auto_refresh") &&
       display->HasEGLExtension("EGL_ANDROID_create_native_client_buffer"));

  // Need EGL_KHR_no_config_context to allow surfaces with and without alpha to
  // be bound to the same context.
  display->egl_no_config_context_supported =
      display->HasEGLExtension("EGL_KHR_no_config_context");

  display->egl_display_texture_share_group_supported =
      display->HasEGLExtension("EGL_ANGLE_display_texture_share_group");
  display->egl_display_semaphore_share_group_supported =
      display->HasEGLExtension("EGL_ANGLE_display_semaphore_share_group");
  display->egl_create_context_client_arrays_supported =
      display->HasEGLExtension("EGL_ANGLE_create_context_client_arrays");
  display->egl_robust_resource_init_supported =
      display->HasEGLExtension("EGL_ANGLE_robust_resource_initialization");

  // Check if SurfacelessEGL is supported.
  display->egl_surfaceless_context_supported =
      display->HasEGLExtension("EGL_KHR_surfaceless_context");

  // TODO(oetuaho@nvidia.com): Surfaceless is disabled on Android as a temporary
  // workaround, since code written for Android WebView takes different paths
  // based on whether GL surface objects have underlying EGL surface handles,
  // conflicting with the use of surfaceless. ANGLE can still expose surfacelss
  // because it is emulated with pbuffers if native support is not present. See
  // https://crbug.com/382349.

#if BUILDFLAG(IS_ANDROID)
  // Use the WebGL compatibility extension for detecting ANGLE. ANGLE always
  // exposes it.
  bool is_angle = display->egl_create_context_webgl_compatability_supported;
  if (!is_angle) {
    display->egl_surfaceless_context_supported = false;
  }
#endif

  if (display->egl_surfaceless_context_supported) {
    // EGL_KHR_surfaceless_context is supported but ensure
    // GL_OES_surfaceless_context is also supported. We need a current context
    // to query for supported GL extensions.
    scoped_refptr<GLSurface> surface =
        new SurfacelessEGL(display, gfx::Size(1, 1));
    scoped_refptr<GLContext> context = InitializeGLContext(
        new GLContextEGL(nullptr), surface.get(), GLContextAttribs());
    if (!context || !context->MakeCurrent(surface.get()))
      display->egl_surfaceless_context_supported = false;

    // Ensure context supports GL_OES_surfaceless_context.
    if (display->egl_surfaceless_context_supported) {
      display->egl_surfaceless_context_supported =
          context->HasExtension("GL_OES_surfaceless_context");
      context->ReleaseCurrent(surface.get());
    }
  }

  // The native fence sync extension is a bit complicated. It's reported as
  // present for ChromeOS, but Android currently doesn't report this extension
  // even when it's present, and older devices and Android emulator may export
  // a useless wrapper function. See crbug.com/775707 for details. In short, if
  // the symbol is present and we're on Android N or newer and we are not on
  // Android emulator, assume that it's usable even if the extension wasn't
  // reported. TODO(https://crbug.com/1086781): Once this is fixed at the
  // Android level, update the heuristic to trust the reported extension from
  // that version onward.
  display->egl_android_native_fence_sync_supported =
      display->HasEGLExtension("EGL_ANDROID_native_fence_sync");
#if BUILDFLAG(IS_ANDROID)
  if (!display->egl_android_native_fence_sync_supported &&
      base::android::BuildInfo::GetInstance()->sdk_int() >=
          base::android::SDK_VERSION_NOUGAT &&
      g_driver_egl.fn.eglDupNativeFenceFDANDROIDFn &&
      base::SysInfo::GetAndroidHardwareEGL() != "swiftshader" &&
      base::SysInfo::GetAndroidHardwareEGL() != "emulation") {
    display->egl_android_native_fence_sync_supported = true;
  }
#endif

  display->egl_ext_pixel_format_float_supported =
      display->HasEGLExtension("EGL_EXT_pixel_format_float");

  display->egl_angle_power_preference_supported =
      display->HasEGLExtension("EGL_ANGLE_power_preference");

  display->egl_angle_external_context_and_surface_supported =
      display->HasEGLExtension("EGL_ANGLE_external_context_and_surface");

  display->egl_ext_query_device_supported =
      display->HasEGLClientExtension("EGL_EXT_device_query");

  display->egl_angle_context_virtualization_supported =
      display->HasEGLExtension("EGL_ANGLE_context_virtualization");

  display->egl_angle_vulkan_image_supported =
      display->HasEGLExtension("EGL_ANGLE_vulkan_image");

  if (display->egl_angle_power_preference_supported) {
    g_egl_gpu_switching_observer = new EGLGpuSwitchingObserver(display);
    ui::GpuSwitchingManager::GetInstance()->AddObserver(
        g_egl_gpu_switching_observer);
  }
}

// static
bool GLSurfaceEGL::InitializeExtensionSettingsOneOff(GLDisplayEGL* display) {
  DCHECK(display);
  if (display->GetDisplay() == EGL_NO_DISPLAY)
    return false;
  g_driver_egl.ext.UpdateConditionalExtensionSettings(display);
  display->egl_client_extensions =
      eglQueryString(EGL_NO_DISPLAY, EGL_EXTENSIONS);
  display->egl_extensions =
      eglQueryString(display->GetDisplay(), EGL_EXTENSIONS);

  return true;
}

// static
void GLSurfaceEGL::ShutdownOneOff(GLDisplayEGL* display) {
  if (!display || display->GetDisplay() == EGL_NO_DISPLAY) {
    return;
  }

  if (g_egl_gpu_switching_observer) {
    ui::GpuSwitchingManager::GetInstance()->RemoveObserver(
        g_egl_gpu_switching_observer);
    delete g_egl_gpu_switching_observer;
    g_egl_gpu_switching_observer = nullptr;
  }
  angle::ResetPlatform(display->GetDisplay());
  DCHECK(g_driver_egl.fn.eglTerminateFn);
  eglTerminate(display->GetDisplay());
  display->SetDisplay(EGL_NO_DISPLAY);

  display->egl_client_extensions = nullptr;
  display->egl_extensions = nullptr;
  display->egl_create_context_robustness_supported = false;
  display->egl_robustness_video_memory_purge_supported = false;
  display->egl_create_context_bind_generates_resource_supported = false;
  display->egl_create_context_webgl_compatability_supported = false;
  display->egl_sync_control_supported = false;
  display->egl_sync_control_rate_supported = false;
  display->egl_window_fixed_size_supported = false;
  display->egl_surface_orientation_supported = false;
  display->egl_surfaceless_context_supported = false;
  display->egl_robust_resource_init_supported = false;
  display->egl_display_texture_share_group_supported = false;
  display->egl_create_context_client_arrays_supported = false;
  display->egl_angle_feature_control_supported = false;
}

GLSurfaceEGL::~GLSurfaceEGL() = default;

// InitializeDisplay is necessary because the static binding code
// needs a full Display init before it can query the Display extensions.
// static
GLDisplayEGL* GLSurfaceEGL::InitializeDisplay(EGLDisplayPlatform native_display,
                                              uint64_t system_device_id) {
  GLDisplayEGL* gl_display =
      GLDisplayManagerEGL::GetInstance()->GetDisplay(system_device_id);
  if (gl_display->GetDisplay() != EGL_NO_DISPLAY) {
    return gl_display;
  }

  gl_display->native_display = native_display;

  // If EGL_EXT_client_extensions not supported this call to eglQueryString
  // will return nullptr.
  gl_display->egl_client_extensions =
      eglQueryString(EGL_NO_DISPLAY, EGL_EXTENSIONS);

  bool supports_egl_debug = gl_display->HasEGLClientExtension("EGL_KHR_debug");
  if (supports_egl_debug) {
    EGLAttrib controls[] = {
        EGL_DEBUG_MSG_CRITICAL_KHR,
        EGL_TRUE,
        EGL_DEBUG_MSG_ERROR_KHR,
        EGL_TRUE,
        EGL_DEBUG_MSG_WARN_KHR,
        EGL_TRUE,
        EGL_DEBUG_MSG_INFO_KHR,
        EGL_TRUE,
        EGL_NONE,
        EGL_NONE,
    };

    eglDebugMessageControlKHR(&LogEGLDebugMessage, controls);
  }

  bool supports_angle_d3d = false;
  bool supports_angle_opengl = false;
  bool supports_angle_null = false;
  bool supports_angle_vulkan = false;
  bool supports_angle_swiftshader = false;
  bool supports_angle_egl = false;
  bool supports_angle_metal = false;
  // Check for availability of ANGLE extensions.
  if (gl_display->HasEGLClientExtension("EGL_ANGLE_platform_angle")) {
    supports_angle_d3d =
        gl_display->HasEGLClientExtension("EGL_ANGLE_platform_angle_d3d");
    supports_angle_opengl =
        gl_display->HasEGLClientExtension("EGL_ANGLE_platform_angle_opengl");
    supports_angle_null =
        gl_display->HasEGLClientExtension("EGL_ANGLE_platform_angle_null");
    supports_angle_vulkan =
        gl_display->HasEGLClientExtension("EGL_ANGLE_platform_angle_vulkan");
    supports_angle_swiftshader = gl_display->HasEGLClientExtension(
        "EGL_ANGLE_platform_angle_device_type_swiftshader");
    supports_angle_egl = gl_display->HasEGLClientExtension(
        "EGL_ANGLE_platform_angle_device_type_egl_angle");
    supports_angle_metal =
        gl_display->HasEGLClientExtension("EGL_ANGLE_platform_angle_metal");
  }

  bool supports_angle = supports_angle_d3d || supports_angle_opengl ||
                        supports_angle_null || supports_angle_vulkan ||
                        supports_angle_swiftshader || supports_angle_metal;

  gl_display->egl_angle_feature_control_supported =
      gl_display->HasEGLClientExtension("EGL_ANGLE_feature_control");

  gl_display->egl_angle_display_power_preference_supported =
      gl_display->HasEGLClientExtension("EGL_ANGLE_display_power_preference");

  gl_display->egl_angle_platform_angle_device_id_supported =
      gl_display->HasEGLClientExtension("EGL_ANGLE_platform_angle_device_id");

  std::vector<DisplayType> init_displays;
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  GetEGLInitDisplays(supports_angle_d3d, supports_angle_opengl,
                     supports_angle_null, supports_angle_vulkan,
                     supports_angle_swiftshader, supports_angle_egl,
                     supports_angle_metal, command_line, &init_displays);

  std::vector<std::string> enabled_angle_features =
      GetStringVectorFromCommandLine(command_line,
                                     switches::kEnableANGLEFeatures);
  std::vector<std::string> disabled_angle_features =
      GetStringVectorFromCommandLine(command_line,
                                     switches::kDisableANGLEFeatures);

  bool disable_all_angle_features =
      command_line->HasSwitch(switches::kDisableGpuDriverBugWorkarounds);

  for (size_t disp_index = 0; disp_index < init_displays.size(); ++disp_index) {
    DisplayType display_type = init_displays[disp_index];
    EGLDisplay egl_display = GetDisplayFromType(
        display_type, gl_display, enabled_angle_features,
        disabled_angle_features, disable_all_angle_features, system_device_id);
    if (egl_display == EGL_NO_DISPLAY) {
      LOG(ERROR) << "EGL display query failed with error "
                 << GetLastEGLErrorString();
    }

    // Init ANGLE platform now that we have the global display.
    if (supports_angle) {
      if (!angle::InitializePlatform(egl_display)) {
        LOG(ERROR) << "ANGLE Platform initialization failed.";
      }

      SetANGLEImplementation(
          GetANGLEImplementationFromDisplayType(display_type));
    }

    // The platform may need to unset its platform specific display env in case
    // of vulkan if the platform doesn't support Vulkan surface.
    absl::optional<base::ScopedEnvironmentVariableOverride> unset_display;
    if (display_type == ANGLE_VULKAN) {
      unset_display = GLDisplayEglUtil::GetInstance()
                          ->MaybeGetScopedDisplayUnsetForVulkan();
    }

    if (!eglInitialize(egl_display, nullptr, nullptr)) {
      bool is_last = disp_index == init_displays.size() - 1;

      LOG(ERROR) << "eglInitialize " << DisplayTypeString(display_type)
                 << " failed with error " << GetLastEGLErrorString()
                 << (is_last ? "" : ", trying next display type");
      continue;
    }

    std::ostringstream display_type_string;
    auto gl_implementation = GetGLImplementationParts();
    display_type_string << GetGLImplementationGLName(gl_implementation);
    if (gl_implementation.gl == kGLImplementationEGLANGLE) {
      display_type_string << ":" << DisplayTypeString(display_type);
    }

    static auto* egl_display_type_key = base::debug::AllocateCrashKeyString(
        "egl-display-type", base::debug::CrashKeySize::Size32);
    base::debug::SetCrashKeyString(egl_display_type_key,
                                   display_type_string.str());

    UMA_HISTOGRAM_ENUMERATION("GPU.EGLDisplayType", display_type,
                              DISPLAY_TYPE_MAX);
    gl_display->SetDisplay(egl_display);
    gl_display->display_type = display_type;
    break;
  }

  return gl_display;
}

NativeViewGLSurfaceEGL::NativeViewGLSurfaceEGL(
    GLDisplayEGL* display,
    EGLNativeWindowType window,
    std::unique_ptr<gfx::VSyncProvider> vsync_provider)
    : GLSurfaceEGL(display),
      window_(window),
      vsync_provider_external_(std::move(vsync_provider)) {
#if BUILDFLAG(IS_ANDROID)
  if (window)
    ANativeWindow_acquire(window);
#endif

#if BUILDFLAG(IS_WIN)
  RECT windowRect;
  if (GetClientRect(window_, &windowRect))
    size_ = gfx::Rect(windowRect).size();
#endif
}

bool NativeViewGLSurfaceEGL::Initialize(GLSurfaceFormat format) {
  DCHECK(!surface_);
  format_ = format;

  if (display_->GetDisplay() == EGL_NO_DISPLAY) {
    LOG(ERROR) << "Trying to create surface with invalid display.";
    return false;
  }

  // We need to make sure that window_ is correctly initialized with all
  // the platform-dependant quirks, if any, before creating the surface.
  if (!InitializeNativeWindow()) {
    LOG(ERROR) << "Error trying to initialize the native window.";
    return false;
  }

  std::vector<EGLint> egl_window_attributes;

  if (display_->egl_window_fixed_size_supported && enable_fixed_size_angle_) {
    egl_window_attributes.push_back(EGL_FIXED_SIZE_ANGLE);
    egl_window_attributes.push_back(EGL_TRUE);
    egl_window_attributes.push_back(EGL_WIDTH);
    egl_window_attributes.push_back(size_.width());
    egl_window_attributes.push_back(EGL_HEIGHT);
    egl_window_attributes.push_back(size_.height());
  }

  if (g_driver_egl.ext.b_EGL_NV_post_sub_buffer) {
    egl_window_attributes.push_back(EGL_POST_SUB_BUFFER_SUPPORTED_NV);
    egl_window_attributes.push_back(EGL_TRUE);
  }

  if (display_->egl_surface_orientation_supported) {
    EGLint attrib;
    eglGetConfigAttrib(display_->GetDisplay(), GetConfig(),
                       EGL_OPTIMAL_SURFACE_ORIENTATION_ANGLE, &attrib);
    surface_origin_ = (attrib == EGL_SURFACE_ORIENTATION_INVERT_Y_ANGLE)
                          ? gfx::SurfaceOrigin::kTopLeft
                          : gfx::SurfaceOrigin::kBottomLeft;
  }

  if (surface_origin_ == gfx::SurfaceOrigin::kTopLeft) {
    egl_window_attributes.push_back(EGL_SURFACE_ORIENTATION_ANGLE);
    egl_window_attributes.push_back(EGL_SURFACE_ORIENTATION_INVERT_Y_ANGLE);
  }

  switch (format_.GetColorSpace()) {
    case GLSurfaceFormat::COLOR_SPACE_UNSPECIFIED:
      break;
    case GLSurfaceFormat::COLOR_SPACE_SRGB:
      // Note that COLORSPACE_LINEAR refers to the sRGB color space, but
      // without opting into sRGB blending. It is equivalent to
      // COLORSPACE_SRGB with Disable(FRAMEBUFFER_SRGB).
      if (display_->egl_khr_colorspace) {
        egl_window_attributes.push_back(EGL_GL_COLORSPACE_KHR);
        egl_window_attributes.push_back(EGL_GL_COLORSPACE_LINEAR_KHR);
      }
      break;
    case GLSurfaceFormat::COLOR_SPACE_DISPLAY_P3:
      // Note that it is not the case that
      //   COLORSPACE_SRGB is to COLORSPACE_LINEAR_KHR
      // as
      //   COLORSPACE_DISPLAY_P3 is to COLORSPACE_DISPLAY_P3_LINEAR
      // COLORSPACE_DISPLAY_P3 is equivalent to COLORSPACE_LINEAR, except with
      // with the P3 gamut instead of the the sRGB gamut.
      // COLORSPACE_DISPLAY_P3_LINEAR has a linear transfer function, and is
      // intended for use with 16-bit formats.
      bool p3_supported = display_->egl_ext_colorspace_display_p3 ||
                          display_->egl_ext_colorspace_display_p3_passthrough;
      if (display_->egl_khr_colorspace && p3_supported) {
        egl_window_attributes.push_back(EGL_GL_COLORSPACE_KHR);
        // Chrome relied on incorrect Android behavior when dealing with P3 /
        // framebuffer_srgb interactions. This behavior was fixed in Q, which
        // causes invalid Chrome rendering. To achieve Android-P behavior in Q+,
        // use EGL_GL_COLORSPACE_P3_PASSTHROUGH_EXT where possible.
        if (display_->egl_ext_colorspace_display_p3_passthrough) {
          egl_window_attributes.push_back(
              EGL_GL_COLORSPACE_DISPLAY_P3_PASSTHROUGH_EXT);
        } else {
          egl_window_attributes.push_back(EGL_GL_COLORSPACE_DISPLAY_P3_EXT);
        }
      }
      break;
  }

  egl_window_attributes.push_back(EGL_NONE);
  // Create a surface for the native window.
  surface_ = eglCreateWindowSurface(display_->GetDisplay(), GetConfig(),
                                    window_, &egl_window_attributes[0]);

  if (!surface_) {
    LOG(ERROR) << "eglCreateWindowSurface failed with error "
               << GetLastEGLErrorString();
    Destroy();
    return false;
  }

  if (g_driver_egl.ext.b_EGL_NV_post_sub_buffer) {
    EGLint surfaceVal;
    EGLBoolean retVal =
        eglQuerySurface(display_->GetDisplay(), surface_,
                        EGL_POST_SUB_BUFFER_SUPPORTED_NV, &surfaceVal);
    supports_post_sub_buffer_ = (surfaceVal && retVal) == EGL_TRUE;
  }

  supports_swap_buffer_with_damage_ =
      g_driver_egl.ext.b_EGL_KHR_swap_buffers_with_damage;

  if (!vsync_provider_external_ &&
      EGLSyncControlVSyncProvider::IsSupported(display_)) {
    vsync_provider_internal_ =
        std::make_unique<EGLSyncControlVSyncProvider>(surface_, display_);
  }

  if (!vsync_provider_external_ && !vsync_provider_internal_)
    vsync_provider_internal_ = CreateVsyncProviderInternal();

  presentation_helper_ =
      std::make_unique<GLSurfacePresentationHelper>(GetVSyncProvider());
  return true;
}

bool NativeViewGLSurfaceEGL::SupportsSwapTimestamps() const {
  return g_driver_egl.ext.b_EGL_ANDROID_get_frame_timestamps;
}

void NativeViewGLSurfaceEGL::SetEnableSwapTimestamps() {
  DCHECK(g_driver_egl.ext.b_EGL_ANDROID_get_frame_timestamps);

  // If frame timestamps are supported, set the proper attribute to enable the
  // feature and then cache the timestamps supported by the underlying
  // implementation. EGL_DISPLAY_PRESENT_TIME_ANDROID support, in particular,
  // is spotty.
  // Clear the supported timestamps here to protect against Initialize() being
  // called twice.
  supported_egl_timestamps_.clear();
  supported_event_names_.clear();
  presentation_feedback_index_ = -1;
  composition_start_index_ = -1;

  eglSurfaceAttrib(display_->GetDisplay(), surface_, EGL_TIMESTAMPS_ANDROID,
                   EGL_TRUE);

  // Check if egl composite interval is supported or not. If not then return.
  // Else check which other timestamps are supported.
  EGLint interval_name = EGL_COMPOSITE_INTERVAL_ANDROID;
  if (!eglGetCompositorTimingSupportedANDROID(display_->GetDisplay(), surface_,
                                              interval_name))
    return;

  static const struct {
    EGLint egl_name;
    const char* name;
  } all_timestamps[kMaxTimestampsSupportable] = {
      {EGL_REQUESTED_PRESENT_TIME_ANDROID, "Queue"},
      {EGL_RENDERING_COMPLETE_TIME_ANDROID, "WritesDone"},
      {EGL_COMPOSITION_LATCH_TIME_ANDROID, "LatchedForDisplay"},
      {EGL_FIRST_COMPOSITION_START_TIME_ANDROID, "1stCompositeCpu"},
      {EGL_LAST_COMPOSITION_START_TIME_ANDROID, "NthCompositeCpu"},
      {EGL_FIRST_COMPOSITION_GPU_FINISHED_TIME_ANDROID, "GpuCompositeDone"},
      {EGL_DISPLAY_PRESENT_TIME_ANDROID, "ScanOutStart"},
      {EGL_DEQUEUE_READY_TIME_ANDROID, "DequeueReady"},
      {EGL_READS_DONE_TIME_ANDROID, "ReadsDone"},
  };

  supported_egl_timestamps_.reserve(kMaxTimestampsSupportable);
  supported_event_names_.reserve(kMaxTimestampsSupportable);
  for (const auto& ts : all_timestamps) {
    if (!eglGetFrameTimestampSupportedANDROID(display_->GetDisplay(), surface_,
                                              ts.egl_name))
      continue;

    // For presentation feedback, prefer the actual scan out time, but fallback
    // to SurfaceFlinger's composite time since some devices don't support
    // the former.
    switch (ts.egl_name) {
      case EGL_FIRST_COMPOSITION_START_TIME_ANDROID:
        // Value of presentation_feedback_index_ relies on the order of
        // all_timestamps.
        presentation_feedback_index_ =
            static_cast<int>(supported_egl_timestamps_.size());
        composition_start_index_ =
            static_cast<int>(supported_egl_timestamps_.size());
        presentation_flags_ = 0;
        break;
      case EGL_DISPLAY_PRESENT_TIME_ANDROID:
        presentation_feedback_index_ =
            static_cast<int>(supported_egl_timestamps_.size());
        presentation_flags_ = gfx::PresentationFeedback::kVSync |
                              gfx::PresentationFeedback::kHWCompletion;
        break;
      case EGL_RENDERING_COMPLETE_TIME_ANDROID:
        writes_done_index_ = static_cast<int>(supported_egl_timestamps_.size());
        break;
    }

    // Stored in separate vectors so we can pass the egl timestamps
    // directly to the EGL functions.
    supported_egl_timestamps_.push_back(ts.egl_name);
    supported_event_names_.push_back(ts.name);
  }
  DCHECK_GE(presentation_feedback_index_, 0);
  DCHECK_GE(composition_start_index_, 0);

  use_egl_timestamps_ = !supported_egl_timestamps_.empty();

  // Recreate the presentation helper here to make sure egl_timestamp_client_
  // in |presentation_helper_| is initialized after |use_egl_timestamp_| is
  // initialized.
  presentation_helper_ =
      std::make_unique<GLSurfacePresentationHelper>(GetVSyncProvider());
}

bool NativeViewGLSurfaceEGL::InitializeNativeWindow() {
  return true;
}

void NativeViewGLSurfaceEGL::Destroy() {
  presentation_helper_ = nullptr;
  vsync_provider_internal_ = nullptr;

  if (surface_) {
    if (!eglDestroySurface(display_->GetDisplay(), surface_)) {
      LOG(ERROR) << "eglDestroySurface failed with error "
                 << GetLastEGLErrorString();
    }
    surface_ = NULL;
  }
}

bool NativeViewGLSurfaceEGL::IsOffscreen() {
  return false;
}

gfx::SwapResult NativeViewGLSurfaceEGL::SwapBuffers(
    PresentationCallback callback) {
  TRACE_EVENT2("gpu", "NativeViewGLSurfaceEGL:RealSwapBuffers",
      "width", GetSize().width(),
      "height", GetSize().height());

  EGLuint64KHR new_frame_id = 0;
  bool new_frame_id_is_valid = true;
  if (use_egl_timestamps_) {
    new_frame_id_is_valid = !!eglGetNextFrameIdANDROID(display_->GetDisplay(),
                                                       surface_, &new_frame_id);
  }
  if (!new_frame_id_is_valid)
    new_frame_id = -1;

  GLSurfacePresentationHelper::ScopedSwapBuffers scoped_swap_buffers(
      presentation_helper_.get(), std::move(callback), new_frame_id);

  if (!eglSwapBuffers(display_->GetDisplay(), surface_)) {
    DVLOG(1) << "eglSwapBuffers failed with error "
             << GetLastEGLErrorString();
    scoped_swap_buffers.set_result(gfx::SwapResult::SWAP_FAILED);
  } else if (use_egl_timestamps_) {
    UpdateSwapEvents(new_frame_id, new_frame_id_is_valid);
  }

  return scoped_swap_buffers.result();
}

void NativeViewGLSurfaceEGL::UpdateSwapEvents(EGLuint64KHR newFrameId,
                                              bool newFrameIdIsValid) {
  // Queue info for the frame just swapped.
  swap_info_queue_.push({newFrameIdIsValid, newFrameId});

  // Make sure we have a frame old enough that all it's timstamps should
  // be available by now.
  constexpr int kFramesAgoToGetServerTimestamps = 4;
  if (swap_info_queue_.size() <= kFramesAgoToGetServerTimestamps)
    return;

  // TraceEvents if needed.
  // If we weren't able to get a valid frame id before the swap, we can't get
  // its timestamps now.
  const SwapInfo& old_swap_info = swap_info_queue_.front();
  if (old_swap_info.frame_id_is_valid && g_trace_swap_enabled.Get().value)
    TraceSwapEvents(old_swap_info.frame_id);

  swap_info_queue_.pop();
}

void NativeViewGLSurfaceEGL::TraceSwapEvents(EGLuint64KHR oldFrameId) {
  // We shouldn't be calling eglGetFrameTimestampsANDROID with more timestamps
  // than it supports.
  DCHECK_LE(supported_egl_timestamps_.size(), kMaxTimestampsSupportable);

  // Get the timestamps.
  std::vector<EGLnsecsANDROID> egl_timestamps(supported_egl_timestamps_.size(),
                                              EGL_TIMESTAMP_INVALID_ANDROID);
  if (!eglGetFrameTimestampsANDROID(
          display_->GetDisplay(), surface_, oldFrameId,
          static_cast<EGLint>(supported_egl_timestamps_.size()),
          supported_egl_timestamps_.data(), egl_timestamps.data())) {
    TRACE_EVENT_INSTANT0("gpu", "eglGetFrameTimestamps:Failed",
                         TRACE_EVENT_SCOPE_THREAD);
    return;
  }

  // Track supported and valid time/name pairs.
  struct TimeNamePair {
    base::TimeTicks time;
    const char* name;
  };

  std::vector<TimeNamePair> tracePairs;
  tracePairs.reserve(supported_egl_timestamps_.size());
  for (size_t i = 0; i < egl_timestamps.size(); i++) {
    // Although a timestamp of 0 is technically valid, we shouldn't expect to
    // see it in practice. 0's are more likely due to a known linux kernel bug
    // that inadvertently discards timestamp information when merging two
    // retired fences.
    if (egl_timestamps[i] == 0 ||
        egl_timestamps[i] == EGL_TIMESTAMP_INVALID_ANDROID ||
        egl_timestamps[i] == EGL_TIMESTAMP_PENDING_ANDROID) {
      continue;
    }
    // TODO(brianderson): Replace FromInternalValue usage.
    tracePairs.push_back(
        {base::TimeTicks::FromInternalValue(
             egl_timestamps[i] / base::TimeTicks::kNanosecondsPerMicrosecond),
         supported_event_names_[i]});
  }
  if (tracePairs.empty()) {
    TRACE_EVENT_INSTANT0("gpu", "TraceSwapEvents:NoValidTimestamps",
                         TRACE_EVENT_SCOPE_THREAD);
    return;
  }

  // Sort the pairs so we can trace them in order.
  std::sort(tracePairs.begin(), tracePairs.end(),
            [](auto& a, auto& b) { return a.time < b.time; });

  // Trace the overall range under which the sub events will be nested.
  // Add an epsilon since the trace viewer interprets timestamp ranges
  // as closed on the left and open on the right. i.e.: [begin, end).
  // The last sub event isn't nested properly without the epsilon.
  auto epsilon = base::Microseconds(1);
  static const char* SwapEvents = "SwapEvents";
  const int64_t trace_id = oldFrameId;
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN_WITH_TIMESTAMP0(
      kSwapEventTraceCategories, SwapEvents, trace_id, tracePairs.front().time);
  TRACE_EVENT_NESTABLE_ASYNC_END_WITH_TIMESTAMP1(
      kSwapEventTraceCategories, SwapEvents, trace_id,
      tracePairs.back().time + epsilon, "id", trace_id);

  // Trace the first event, which does not have a range before it.
  TRACE_EVENT_NESTABLE_ASYNC_INSTANT_WITH_TIMESTAMP0(
      kSwapEventTraceCategories, tracePairs.front().name, trace_id,
      tracePairs.front().time);

  // Trace remaining events and their ranges.
  // Use the first characters to represent events still pending.
  // This helps color code the remaining events in the viewer, which makes
  // it obvious:
  //   1) when the order of events are different between frames and
  //   2) if multiple events occurred very close together.
  std::string valid_symbols(tracePairs.size(), '\0');
  for (size_t i = 0; i < valid_symbols.size(); i++)
    valid_symbols[i] = tracePairs[i].name[0];

  const char* pending_symbols = valid_symbols.c_str();
  for (size_t i = 1; i < tracePairs.size(); i++) {
    pending_symbols++;
    TRACE_EVENT_COPY_NESTABLE_ASYNC_BEGIN_WITH_TIMESTAMP0(
        kSwapEventTraceCategories, pending_symbols, trace_id,
        tracePairs[i - 1].time);
    TRACE_EVENT_COPY_NESTABLE_ASYNC_END_WITH_TIMESTAMP0(
        kSwapEventTraceCategories, pending_symbols, trace_id,
        tracePairs[i].time);
    TRACE_EVENT_NESTABLE_ASYNC_INSTANT_WITH_TIMESTAMP0(
        kSwapEventTraceCategories, tracePairs[i].name, trace_id,
        tracePairs[i].time);
  }
}

std::unique_ptr<gfx::VSyncProvider>
NativeViewGLSurfaceEGL::CreateVsyncProviderInternal() {
  return nullptr;
}

gfx::Size NativeViewGLSurfaceEGL::GetSize() {
  EGLint width;
  EGLint height;
  if (!eglQuerySurface(display_->GetDisplay(), surface_, EGL_WIDTH, &width) ||
      !eglQuerySurface(display_->GetDisplay(), surface_, EGL_HEIGHT, &height)) {
    NOTREACHED() << "eglQuerySurface failed with error "
                 << GetLastEGLErrorString();
    return gfx::Size();
  }

  return gfx::Size(width, height);
}

bool NativeViewGLSurfaceEGL::Resize(const gfx::Size& size,
                                    float scale_factor,
                                    const gfx::ColorSpace& color_space,
                                    bool has_alpha) {
  if (size == GetSize())
    return true;
  size_ = size;
  GLContext* context = GLContext::GetCurrent();
  DCHECK(context);
  GLSurface* surface = GLSurface::GetCurrent();
  DCHECK(surface);
  // Current surface may not be |this| if it is wrapped, but it should point to
  // the same handle.
  DCHECK_EQ(surface->GetHandle(), GetHandle());
  context->ReleaseCurrent(surface);
  Destroy();
  if (!Initialize(format_)) {
    LOG(ERROR) << "Failed to resize window.";
    return false;
  }
  if (!context->MakeCurrent(surface)) {
    LOG(ERROR) << "Failed to make current in NativeViewGLSurfaceEGL::Resize";
    return false;
  }
  SetVSyncEnabled(vsync_enabled_);
  if (use_egl_timestamps_) {
    eglSurfaceAttrib(display_->GetDisplay(), surface_, EGL_TIMESTAMPS_ANDROID,
                     EGL_TRUE);
  }
  return true;
}

bool NativeViewGLSurfaceEGL::Recreate() {
  GLContext* context = GLContext::GetCurrent();
  DCHECK(context);
  GLSurface* surface = GLSurface::GetCurrent();
  DCHECK(surface);
  // Current surface may not be |this| if it is wrapped, but it should point to
  // the same handle.
  DCHECK_EQ(surface->GetHandle(), GetHandle());
  context->ReleaseCurrent(surface);
  Destroy();
  if (!Initialize(format_)) {
    LOG(ERROR) << "Failed to create surface.";
    return false;
  }
  if (!context->MakeCurrent(surface)) {
    LOG(ERROR) << "Failed to make current in NativeViewGLSurfaceEGL::Recreate";
    return false;
  }
  SetVSyncEnabled(vsync_enabled_);
  if (use_egl_timestamps_) {
    eglSurfaceAttrib(display_->GetDisplay(), surface_, EGL_TIMESTAMPS_ANDROID,
                     EGL_TRUE);
  }
  return true;
}

EGLSurface NativeViewGLSurfaceEGL::GetHandle() {
  return surface_;
}

bool NativeViewGLSurfaceEGL::SupportsPostSubBuffer() {
  return supports_post_sub_buffer_;
}

gfx::SurfaceOrigin NativeViewGLSurfaceEGL::GetOrigin() const {
  return surface_origin_;
}

EGLTimestampClient* NativeViewGLSurfaceEGL::GetEGLTimestampClient() {
  // This api call is used by GLSurfacePresentationHelper class which is member
  // of this class NativeViewGLSurfaceEGL. Hence its guaranteed "this" pointer
  // will live longer than the GLSurfacePresentationHelper class.
  return this;
}

bool NativeViewGLSurfaceEGL::IsEGLTimestampSupported() const {
  return use_egl_timestamps_;
}

bool NativeViewGLSurfaceEGL::GetFrameTimestampInfoIfAvailable(
    base::TimeTicks* presentation_time,
    base::TimeDelta* composite_interval,
    base::TimeTicks* writes_done_time,
    uint32_t* presentation_flags,
    int frame_id) {
  DCHECK(presentation_time);
  DCHECK(composite_interval);
  DCHECK(presentation_flags);

  TRACE_EVENT1("gpu", "NativeViewGLSurfaceEGL:GetFrameTimestampInfoIfAvailable",
               "frame_id", frame_id);

  // Get the composite interval.
  EGLint interval_name = EGL_COMPOSITE_INTERVAL_ANDROID;
  EGLnsecsANDROID composite_interval_ns = 0;
  *presentation_flags = 0;

  // If an error is generated, we will treat it as a frame done for timestamp
  // reporting purpose.
  if (!eglGetCompositorTimingANDROID(GetEGLDisplay(), surface_, 1,
                                     &interval_name, &composite_interval_ns)) {
    *composite_interval =
        base::Nanoseconds(base::TimeTicks::kNanosecondsPerSecond / 60);
    // If we couldn't get the correct presentation time due to some errors,
    // return the current time.
    *presentation_time = base::TimeTicks::Now();
    return true;
  }

  // If the composite interval is pending, the frame is not yet done.
  if (composite_interval_ns == EGL_TIMESTAMP_PENDING_ANDROID) {
    return false;
  }
  DCHECK_GT(composite_interval_ns, 0);
  *composite_interval = base::Nanoseconds(composite_interval_ns);

  // Get the all available timestamps for the frame. If a frame is invalid or
  // an error is generated,  we will treat it as a frame done for timestamp
  // reporting purpose.
  std::vector<EGLnsecsANDROID> egl_timestamps(supported_egl_timestamps_.size(),
                                              EGL_TIMESTAMP_INVALID_ANDROID);

  // TODO(vikassoni): File a driver bug for eglGetFrameTimestampsANDROID().
  // See https://bugs.chromium.org/p/chromium/issues/detail?id=966638.
  // As per the spec, the driver is expected to return a valid timestamp from
  // the call eglGetFrameTimestampsANDROID() when its not
  // EGL_TIMESTAMP_PENDING_ANDROID or EGL_TIMESTAMP_INVALID_ANDROID. But
  // currently some buggy drivers an invalid timestamp 0.
  // This is currentlt handled in chrome for by setting the presentation time to
  // TimeTicks::Now() (snapped to the next vsync) instead of 0.
  if ((frame_id < 0) ||
      !eglGetFrameTimestampsANDROID(
          display_->GetDisplay(), surface_, frame_id,
          static_cast<EGLint>(supported_egl_timestamps_.size()),
          supported_egl_timestamps_.data(), egl_timestamps.data())) {
    // If we couldn't get the correct presentation time due to some errors,
    // return the current time.
    *presentation_time = base::TimeTicks::Now();
    return true;
  }
  DCHECK_GE(presentation_feedback_index_, 0);
  DCHECK_GE(composition_start_index_, 0);

  // Get the presentation time.
  EGLnsecsANDROID presentation_time_ns =
      egl_timestamps[presentation_feedback_index_];

  // If the presentation time is pending, the frame is not yet done.
  if (presentation_time_ns == EGL_TIMESTAMP_PENDING_ANDROID) {
    return false;
  }
  if (presentation_time_ns == EGL_TIMESTAMP_INVALID_ANDROID) {
    presentation_time_ns = egl_timestamps[composition_start_index_];
    if (presentation_time_ns == EGL_TIMESTAMP_INVALID_ANDROID ||
        presentation_time_ns == EGL_TIMESTAMP_PENDING_ANDROID) {
      *presentation_time = base::TimeTicks::Now();
    } else {
      *presentation_time =
          base::TimeTicks() + base::Nanoseconds(presentation_time_ns);
    }
  } else {
    *presentation_time =
        base::TimeTicks() + base::Nanoseconds(presentation_time_ns);
    *presentation_flags = presentation_flags_;
  }

  // Get the WritesDone time if available, otherwise set to a null TimeTicks.
  EGLnsecsANDROID writes_done_time_ns = egl_timestamps[writes_done_index_];
  if (writes_done_time_ns == EGL_TIMESTAMP_INVALID_ANDROID ||
      writes_done_time_ns == EGL_TIMESTAMP_PENDING_ANDROID) {
    *writes_done_time = base::TimeTicks();
  } else {
    *writes_done_time =
        base::TimeTicks() + base::Nanoseconds(writes_done_time_ns);
  }

  return true;
}

gfx::SwapResult NativeViewGLSurfaceEGL::SwapBuffersWithDamage(
    const std::vector<int>& rects,
    PresentationCallback callback) {
  DCHECK(supports_swap_buffer_with_damage_);

  GLSurfacePresentationHelper::ScopedSwapBuffers scoped_swap_buffers(
      presentation_helper_.get(), std::move(callback));
  if (!eglSwapBuffersWithDamageKHR(display_->GetDisplay(), surface_,
                                   const_cast<EGLint*>(rects.data()),
                                   static_cast<EGLint>(rects.size() / 4))) {
    DVLOG(1) << "eglSwapBuffersWithDamageKHR failed with error "
             << GetLastEGLErrorString();
    scoped_swap_buffers.set_result(gfx::SwapResult::SWAP_FAILED);
  }
  return scoped_swap_buffers.result();
}

gfx::SwapResult NativeViewGLSurfaceEGL::PostSubBuffer(
    int x,
    int y,
    int width,
    int height,
    PresentationCallback callback) {
  TRACE_EVENT2("gpu", "NativeViewGLSurfaceEGL:PostSubBuffer", "width", width,
               "height", height);
  DCHECK(supports_post_sub_buffer_);
  if (surface_origin_ == gfx::SurfaceOrigin::kTopLeft) {
    // With EGL_SURFACE_ORIENTATION_INVERT_Y_ANGLE the contents are rendered
    // inverted, but the PostSubBuffer rectangle is still measured from the
    // bottom left.
    y = GetSize().height() - y - height;
  }

  GLSurfacePresentationHelper::ScopedSwapBuffers scoped_swap_buffers(
      presentation_helper_.get(), std::move(callback));
  if (!eglPostSubBufferNV(GetEGLDisplay(), surface_, x, y, width, height)) {
    DVLOG(1) << "eglPostSubBufferNV failed with error "
             << GetLastEGLErrorString();
    scoped_swap_buffers.set_result(gfx::SwapResult::SWAP_FAILED);
  }
  return scoped_swap_buffers.result();
}

bool NativeViewGLSurfaceEGL::SupportsCommitOverlayPlanes() {
  return false;
}

gfx::SwapResult NativeViewGLSurfaceEGL::CommitOverlayPlanes(
    PresentationCallback callback) {
  NOTREACHED();
  return gfx::SwapResult::SWAP_FAILED;
}

bool NativeViewGLSurfaceEGL::OnMakeCurrent(GLContext* context) {
  if (presentation_helper_)
    presentation_helper_->OnMakeCurrent(context, this);
  return GLSurfaceEGL::OnMakeCurrent(context);
}

gfx::VSyncProvider* NativeViewGLSurfaceEGL::GetVSyncProvider() {
  return vsync_provider_external_ ? vsync_provider_external_.get()
                                  : vsync_provider_internal_.get();
}

void NativeViewGLSurfaceEGL::SetVSyncEnabled(bool enabled) {
  DCHECK(GLContext::GetCurrent() && GLContext::GetCurrent()->IsCurrent(this));
  vsync_enabled_ = enabled;
  if (!eglSwapInterval(display_->GetDisplay(), enabled ? 1 : 0)) {
    LOG(ERROR) << "eglSwapInterval failed with error "
               << GetLastEGLErrorString();
  }
}

bool NativeViewGLSurfaceEGL::ScheduleOverlayPlane(
    GLImage* image,
    std::unique_ptr<gfx::GpuFence> gpu_fence,
    const gfx::OverlayPlaneData& overlay_plane_data) {
  NOTIMPLEMENTED();
  return false;
}

NativeViewGLSurfaceEGL::~NativeViewGLSurfaceEGL() {
  Destroy();
#if BUILDFLAG(IS_ANDROID)
  if (window_)
    ANativeWindow_release(window_);
#endif
}

PbufferGLSurfaceEGL::PbufferGLSurfaceEGL(GLDisplayEGL* display,
                                         const gfx::Size& size)
    : GLSurfaceEGL(display), size_(size), surface_(nullptr) {
  // Some implementations of Pbuffer do not support having a 0 size. For such
  // cases use a (1, 1) surface.
  if (size_.GetArea() == 0)
    size_.SetSize(1, 1);
}

bool PbufferGLSurfaceEGL::Initialize(GLSurfaceFormat format) {
  EGLSurface old_surface = surface_;

#if BUILDFLAG(IS_ANDROID)
  // This is to allow context virtualization which requires on- and offscreen
  // to use a compatible config. We expect the client to request RGB565
  // onscreen surface also for this to work (with the exception of
  // fullscreen video).
  if (base::SysInfo::AmountOfPhysicalMemoryMB() <= 512)
    format.SetRGB565();
#endif

  format_ = format;

  // Allocate the new pbuffer surface before freeing the old one to ensure
  // they have different addresses. If they have the same address then a
  // future call to MakeCurrent might early out because it appears the current
  // context and surface have not changed.
  std::vector<EGLint> pbuffer_attribs;
  pbuffer_attribs.push_back(EGL_WIDTH);
  pbuffer_attribs.push_back(size_.width());
  pbuffer_attribs.push_back(EGL_HEIGHT);
  pbuffer_attribs.push_back(size_.height());

  // Enable robust resource init when using SwANGLE
  if (IsSoftwareGLImplementation(GetGLImplementationParts()) &&
      display_->IsRobustResourceInitSupported()) {
    pbuffer_attribs.push_back(EGL_ROBUST_RESOURCE_INITIALIZATION_ANGLE);
    pbuffer_attribs.push_back(EGL_TRUE);
  }

  // Append final EGL_NONE to signal the pbuffer attributes are finished
  pbuffer_attribs.push_back(EGL_NONE);
  pbuffer_attribs.push_back(EGL_NONE);

  EGLSurface new_surface = eglCreatePbufferSurface(
      display_->GetDisplay(), GetConfig(), &pbuffer_attribs[0]);
  if (!new_surface) {
    LOG(ERROR) << "eglCreatePbufferSurface failed with error "
               << GetLastEGLErrorString();
    return false;
  }

  if (old_surface)
    eglDestroySurface(display_->GetDisplay(), old_surface);

  surface_ = new_surface;
  return true;
}

void PbufferGLSurfaceEGL::Destroy() {
  if (surface_) {
    if (!eglDestroySurface(display_->GetDisplay(), surface_)) {
      LOG(ERROR) << "eglDestroySurface failed with error "
                 << GetLastEGLErrorString();
    }
    surface_ = NULL;
  }
}

bool PbufferGLSurfaceEGL::IsOffscreen() {
  return true;
}

gfx::SwapResult PbufferGLSurfaceEGL::SwapBuffers(
    PresentationCallback callback) {
  NOTREACHED() << "Attempted to call SwapBuffers on a PbufferGLSurfaceEGL.";
  return gfx::SwapResult::SWAP_FAILED;
}

gfx::Size PbufferGLSurfaceEGL::GetSize() {
  return size_;
}

bool PbufferGLSurfaceEGL::Resize(const gfx::Size& size,
                                 float scale_factor,
                                 const gfx::ColorSpace& color_space,
                                 bool has_alpha) {
  if (size == size_)
    return true;

  size_ = size;

  GLContext* context = GLContext::GetCurrent();
  DCHECK(context);
  GLSurface* surface = GLSurface::GetCurrent();
  DCHECK(surface);
  // Current surface may not be |this| if it is wrapped, but it should point to
  // the same handle.
  DCHECK_EQ(surface->GetHandle(), GetHandle());
  context->ReleaseCurrent(surface);

  if (!Initialize(format_)) {
    LOG(ERROR) << "Failed to resize pbuffer.";
    return false;
  }

  if (!context->MakeCurrent(surface)) {
    LOG(ERROR) << "Failed to make current in PbufferGLSurfaceEGL::Resize";
    return false;
  }

  return true;
}

EGLSurface PbufferGLSurfaceEGL::GetHandle() {
  return surface_;
}

void* PbufferGLSurfaceEGL::GetShareHandle() {
#if BUILDFLAG(IS_ANDROID)
  NOTREACHED();
  return nullptr;
#else
  if (!g_driver_egl.ext.b_EGL_ANGLE_query_surface_pointer)
    return nullptr;

  if (!g_driver_egl.ext.b_EGL_ANGLE_surface_d3d_texture_2d_share_handle)
    return nullptr;

  void* handle;
  if (!eglQuerySurfacePointerANGLE(display_->GetDisplay(), GetHandle(),
                                   EGL_D3D_TEXTURE_2D_SHARE_HANDLE_ANGLE,
                                   &handle)) {
    return nullptr;
  }

  return handle;
#endif
}

PbufferGLSurfaceEGL::~PbufferGLSurfaceEGL() {
  Destroy();
}

SurfacelessEGL::SurfacelessEGL(GLDisplayEGL* display, const gfx::Size& size)
    : GLSurfaceEGL(display), size_(size) {}

bool SurfacelessEGL::Initialize(GLSurfaceFormat format) {
  format_ = format;
  return true;
}

void SurfacelessEGL::Destroy() {
}

bool SurfacelessEGL::IsOffscreen() {
  return true;
}

bool SurfacelessEGL::IsSurfaceless() const {
  return true;
}

gfx::SwapResult SurfacelessEGL::SwapBuffers(PresentationCallback callback) {
  LOG(ERROR) << "Attempted to call SwapBuffers with SurfacelessEGL.";
  return gfx::SwapResult::SWAP_FAILED;
}

gfx::Size SurfacelessEGL::GetSize() {
  return size_;
}

bool SurfacelessEGL::Resize(const gfx::Size& size,
                            float scale_factor,
                            const gfx::ColorSpace& color_space,
                            bool has_alpha) {
  size_ = size;
  return true;
}

EGLSurface SurfacelessEGL::GetHandle() {
  return EGL_NO_SURFACE;
}

void* SurfacelessEGL::GetShareHandle() {
  return nullptr;
}

SurfacelessEGL::~SurfacelessEGL() {
}

}  // namespace gl
