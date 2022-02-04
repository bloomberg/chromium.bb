// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/direct_composition_surface_win.h"

#include <dxgi1_6.h>

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/no_destructor.h"
#include "base/synchronization/lock.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/traced_value.h"
#include "base/win/windows_version.h"
#include "ui/gl/dc_layer_tree.h"
#include "ui/gl/direct_composition_child_surface_win.h"
#include "ui/gl/gl_angle_util_win.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_features.h"
#include "ui/gl/gl_implementation.h"
#include "ui/gl/gl_switches.h"
#include "ui/gl/gpu_switching_manager.h"

namespace gl {
namespace {
// Whether the overlay caps are valid or not. GUARDED_BY GetOverlayLock().
bool g_overlay_caps_valid = false;
// Indicates support for either NV12 or YUY2 overlays. GUARDED_BY
// GetOverlayLock().
bool g_supports_overlays = false;
// Whether the GPU can support hardware overlays or not.
bool g_supports_hardware_overlays = false;
// Whether the DecodeSwapChain is disabled or not.
bool g_decode_swap_chain_disabled = false;
// Whether to force the nv12 overlay support.
bool g_force_nv12_overlay_support = false;
// Whether software overlays have been disabled.
bool g_disable_sw_overlays = false;

// The lock to guard g_overlay_caps_valid and g_supports_overlays.
base::Lock& GetOverlayLock() {
  static base::NoDestructor<base::Lock> overlay_lock;
  return *overlay_lock;
}

bool SupportsOverlays() {
  base::AutoLock auto_lock(GetOverlayLock());
  return g_supports_overlays;
}

bool SupportsHardwareOverlays() {
  base::AutoLock auto_lock(GetOverlayLock());
  return g_supports_hardware_overlays;
}

void SetSupportsOverlays(bool support) {
  base::AutoLock auto_lock(GetOverlayLock());
  g_supports_overlays = support;
}

void SetSupportsHardwareOverlays(bool support) {
  base::AutoLock auto_lock(GetOverlayLock());
  g_supports_hardware_overlays = support;
}

bool SupportsSoftwareOverlays() {
  return base::FeatureList::IsEnabled(
             features::kDirectCompositionSoftwareOverlays) &&
         !g_disable_sw_overlays;
}

bool OverlayCapsValid() {
  base::AutoLock auto_lock(GetOverlayLock());
  return g_overlay_caps_valid;
}
void SetOverlayCapsValid(bool valid) {
  base::AutoLock auto_lock(GetOverlayLock());
  g_overlay_caps_valid = valid;
}
// A warpper of IDXGIOutput4::CheckOverlayColorSpaceSupport()
bool CheckOverlayColorSpaceSupport(
    DXGI_FORMAT dxgi_format,
    DXGI_COLOR_SPACE_TYPE dxgi_color_space,
    Microsoft::WRL::ComPtr<IDXGIOutput> output,
    Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device) {
  UINT color_space_support_flags = 0;
  Microsoft::WRL::ComPtr<IDXGIOutput4> output4;
  if (FAILED(output.As(&output4)) ||
      FAILED(output4->CheckOverlayColorSpaceSupport(
          dxgi_format, dxgi_color_space, d3d11_device.Get(),
          &color_space_support_flags)))
    return false;
  return (color_space_support_flags &
          DXGI_OVERLAY_COLOR_SPACE_SUPPORT_FLAG_PRESENT);
}

// Used for adjusting overlay size to monitor size.
gfx::Size g_primary_monitor_size;

// The number of all visible display monitors on a desktop.
int g_num_of_monitors = 0;

Microsoft::WRL::ComPtr<IDCompositionDevice2> g_dcomp_device;

DirectCompositionSurfaceWin::OverlayHDRInfoUpdateCallback
    g_overlay_hdr_gpu_info_callback;

// Preferred overlay format set when detecting overlay support during
// initialization.  Set to NV12 by default so that it's used when enabling
// overlays using command line flags.
DXGI_FORMAT g_overlay_format_used = DXGI_FORMAT_NV12;
DXGI_FORMAT g_overlay_format_used_hdr = DXGI_FORMAT_UNKNOWN;

// These are the raw support info, which shouldn't depend on field trial state,
// or command line flags. GUARDED_BY GetOverlayLock().
UINT g_nv12_overlay_support_flags = 0;
UINT g_yuy2_overlay_support_flags = 0;
UINT g_bgra8_overlay_support_flags = 0;
UINT g_rgb10a2_overlay_support_flags = 0;

// When this is set, if NV12 or YUY2 overlays are supported, set BGRA8 overlays
// as supported as well.
bool g_enable_bgra8_overlays_with_yuv_overlay_support = false;

// Force enabling DXGI_FORMAT_R10G10B10A2_UNORM format for overlay. Intel
// celake and Tigerlake fail to report the cap of this HDR overlay format.
// TODO(magchen@): Remove this workaround when this cap is fixed in the Intel
// drivers.
bool g_force_rgb10a2_overlay_support = false;

// Per Intel's request, only use NV12 for overlay when
// COLOR_SPACE_YCBCR_STUDIO_G22_LEFT_P709 is also supported. At least one Intel
// Gen9 SKU does not support NV12 overlays and it cannot be screened by the
// device id.
bool g_check_ycbcr_studio_g22_left_p709_for_nv12_support = false;

void SetOverlaySupportFlagsForFormats(UINT nv12_flags,
                                      UINT yuy2_flags,
                                      UINT bgra8_flags,
                                      UINT rgb10a2_flags) {
  base::AutoLock auto_lock(GetOverlayLock());
  g_nv12_overlay_support_flags = nv12_flags;
  g_yuy2_overlay_support_flags = yuy2_flags;
  g_bgra8_overlay_support_flags = bgra8_flags;
  g_rgb10a2_overlay_support_flags = rgb10a2_flags;
}

bool FlagsSupportsOverlays(UINT flags) {
  return (flags & (DXGI_OVERLAY_SUPPORT_FLAG_DIRECT |
                   DXGI_OVERLAY_SUPPORT_FLAG_SCALING));
}

void GetGpuDriverOverlayInfo(bool* supports_overlays,
                             bool* supports_hardware_overlays,
                             DXGI_FORMAT* overlay_format_used,
                             DXGI_FORMAT* overlay_format_used_hdr,
                             UINT* nv12_overlay_support_flags,
                             UINT* yuy2_overlay_support_flags,
                             UINT* bgra8_overlay_support_flags,
                             UINT* rgb10a2_overlay_support_flags) {
  // Initialization
  *supports_overlays = false;
  *supports_hardware_overlays = false;
  *overlay_format_used = DXGI_FORMAT_NV12;
  *overlay_format_used_hdr = DXGI_FORMAT_R10G10B10A2_UNORM;
  *nv12_overlay_support_flags = 0;
  *yuy2_overlay_support_flags = 0;
  *bgra8_overlay_support_flags = 0;
  *rgb10a2_overlay_support_flags = 0;

  // Check for DirectComposition support first to prevent likely crashes.
  if (!DirectCompositionSurfaceWin::IsDirectCompositionSupported())
    return;

  // Before Windows 10 Anniversary Update (Redstone 1), overlay planes wouldn't
  // be assigned to non-UWP apps.
  if (base::win::GetVersion() < base::win::Version::WIN10_RS1)
    return;

  Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device =
      QueryD3D11DeviceObjectFromANGLE();
  if (!d3d11_device) {
    DLOG(ERROR) << "Failed to retrieve D3D11 device";
    return;
  }

  Microsoft::WRL::ComPtr<IDXGIDevice> dxgi_device;
  if (FAILED(d3d11_device.As(&dxgi_device))) {
    DLOG(ERROR) << "Failed to retrieve DXGI device";
    return;
  }

  Microsoft::WRL::ComPtr<IDXGIAdapter> dxgi_adapter;
  if (FAILED(dxgi_device->GetAdapter(&dxgi_adapter))) {
    DLOG(ERROR) << "Failed to retrieve DXGI adapter";
    return;
  }

  // This will fail if the D3D device is "Microsoft Basic Display Adapter".
  Microsoft::WRL::ComPtr<ID3D11VideoDevice> video_device;
  if (FAILED(d3d11_device.As(&video_device))) {
    DLOG(ERROR) << "Failed to retrieve video device";
    return;
  }

  unsigned int i = 0;
  while (true) {
    Microsoft::WRL::ComPtr<IDXGIOutput> output;
    if (FAILED(dxgi_adapter->EnumOutputs(i++, &output)))
      break;
    DCHECK(output);
    Microsoft::WRL::ComPtr<IDXGIOutput3> output3;
    if (FAILED(output.As(&output3)))
      continue;
    DCHECK(output3);
    output3->CheckOverlaySupport(DXGI_FORMAT_NV12, d3d11_device.Get(),
                                 nv12_overlay_support_flags);
    output3->CheckOverlaySupport(DXGI_FORMAT_YUY2, d3d11_device.Get(),
                                 yuy2_overlay_support_flags);
    output3->CheckOverlaySupport(DXGI_FORMAT_B8G8R8A8_UNORM, d3d11_device.Get(),
                                 bgra8_overlay_support_flags);
    // Today it still returns false, which blocks Chrome from using HDR
    // overlays.
    output3->CheckOverlaySupport(DXGI_FORMAT_R10G10B10A2_UNORM,
                                 d3d11_device.Get(),
                                 rgb10a2_overlay_support_flags);
    if (FlagsSupportsOverlays(*nv12_overlay_support_flags)) {
      // NV12 format is preferred if it's supported.
      *overlay_format_used = DXGI_FORMAT_NV12;
      *supports_hardware_overlays = true;

      if (g_check_ycbcr_studio_g22_left_p709_for_nv12_support &&
          !CheckOverlayColorSpaceSupport(
              DXGI_FORMAT_NV12, DXGI_COLOR_SPACE_YCBCR_STUDIO_G22_LEFT_P709,
              output, d3d11_device)) {
        // Some new Intel drivers only claim to support unscaled overlays, but
        // scaled overlays still work. It's possible DWM works around it by
        // performing an extra scaling Blt before calling the driver. Even when
        // scaled overlays aren't actually supported, presentation using the
        // overlay path should be relatively efficient.
        *supports_hardware_overlays = false;
      }
    }
    if (!*supports_hardware_overlays &&
        FlagsSupportsOverlays(*yuy2_overlay_support_flags)) {
      // If NV12 isn't supported, fallback to YUY2 if it's supported.
      *overlay_format_used = DXGI_FORMAT_YUY2;
      *supports_hardware_overlays = true;
    }
    if (g_enable_bgra8_overlays_with_yuv_overlay_support) {
      if (FlagsSupportsOverlays(*nv12_overlay_support_flags))
        *bgra8_overlay_support_flags = *nv12_overlay_support_flags;
      else if (FlagsSupportsOverlays(*yuy2_overlay_support_flags))
        *bgra8_overlay_support_flags = *yuy2_overlay_support_flags;
    }

    // RGB10A2 overlay is used for displaying HDR content. In Intel's
    // platform, RGB10A2 overlay is enabled only when
    // DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020 is supported.
    if (FlagsSupportsOverlays(*rgb10a2_overlay_support_flags)) {
      if (!CheckOverlayColorSpaceSupport(
              DXGI_FORMAT_R10G10B10A2_UNORM,
              DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020, output, d3d11_device))
        *rgb10a2_overlay_support_flags = 0;
    }
    if (g_force_rgb10a2_overlay_support) {
      *rgb10a2_overlay_support_flags = DXGI_OVERLAY_SUPPORT_FLAG_SCALING;
    }

    // Early out after the first output that reports overlay support. All
    // outputs are expected to report the same overlay support according to
    // Microsoft's WDDM documentation:
    // https://docs.microsoft.com/en-us/windows-hardware/drivers/display/multiplane-overlay-hardware-requirements
    // TODO(sunnyps): If the above is true, then we can only look at first
    // output instead of iterating over all outputs.
    if (*supports_hardware_overlays)
      break;
  }

  *supports_overlays = *supports_hardware_overlays;
  if (*supports_hardware_overlays || !SupportsSoftwareOverlays()) {
    return;
  }

  // If no devices with hardware overlay support were found use software ones.
  *supports_overlays = true;
  *nv12_overlay_support_flags = 0;
  *yuy2_overlay_support_flags = 0;
  *bgra8_overlay_support_flags = 0;
  *rgb10a2_overlay_support_flags = 0;

  // Software overlays always use NV12 because it's slightly more efficient and
  // YUY2 was only used because Skylake doesn't support NV12 hardware overlays.
  *overlay_format_used = DXGI_FORMAT_NV12;
}

void UpdateOverlaySupport() {
  if (OverlayCapsValid())
    return;
  SetOverlayCapsValid(true);

  bool supports_overlays = false;
  bool supports_hardware_overlays = false;
  DXGI_FORMAT overlay_format_used = DXGI_FORMAT_NV12;
  DXGI_FORMAT overlay_format_used_hdr = DXGI_FORMAT_R10G10B10A2_UNORM;
  UINT nv12_overlay_support_flags = 0;
  UINT yuy2_overlay_support_flags = 0;
  UINT bgra8_overlay_support_flags = 0;
  UINT rgb10a2_overlay_support_flags = 0;

  GetGpuDriverOverlayInfo(
      &supports_overlays, &supports_hardware_overlays, &overlay_format_used,
      &overlay_format_used_hdr, &nv12_overlay_support_flags,
      &yuy2_overlay_support_flags, &bgra8_overlay_support_flags,
      &rgb10a2_overlay_support_flags);

  if (g_force_nv12_overlay_support) {
    supports_overlays = true;
    nv12_overlay_support_flags = DXGI_OVERLAY_SUPPORT_FLAG_SCALING;
    overlay_format_used = DXGI_FORMAT_NV12;
  }

  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDirectCompositionVideoSwapChainFormat)) {
    std::string override_format =
        base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
            switches::kDirectCompositionVideoSwapChainFormat);
    if (override_format == kSwapChainFormatNV12) {
      overlay_format_used = DXGI_FORMAT_NV12;
    } else if (override_format == kSwapChainFormatYUY2) {
      overlay_format_used = DXGI_FORMAT_YUY2;
    } else if (override_format == kSwapChainFormatBGRA) {
      overlay_format_used = DXGI_FORMAT_B8G8R8A8_UNORM;
    } else {
      DLOG(ERROR) << "Invalid value for switch "
                  << switches::kDirectCompositionVideoSwapChainFormat;
    }
  }

  // Record histograms.
  if (supports_overlays) {
    base::UmaHistogramSparse("GPU.DirectComposition.OverlayFormatUsed3",
                             overlay_format_used);
  }
  base::UmaHistogramBoolean("GPU.DirectComposition.OverlaysSupported",
                            supports_overlays);
  base::UmaHistogramBoolean("GPU.DirectComposition.HardwareOverlaysSupported",
                            supports_hardware_overlays);

  // Update global caps.
  SetSupportsOverlays(supports_overlays);
  SetSupportsHardwareOverlays(supports_hardware_overlays);
  SetOverlaySupportFlagsForFormats(
      nv12_overlay_support_flags, yuy2_overlay_support_flags,
      bgra8_overlay_support_flags, rgb10a2_overlay_support_flags);
  g_overlay_format_used = overlay_format_used;
  g_overlay_format_used_hdr = overlay_format_used_hdr;
}

void RunOverlayHdrGpuInfoUpdateCallback() {
  if (g_overlay_hdr_gpu_info_callback)
    g_overlay_hdr_gpu_info_callback.Run();
}

void UpdateMonitorInfo() {
  g_num_of_monitors = GetSystemMetrics(SM_CMONITORS);

  MONITORINFO monitor_info;
  monitor_info.cbSize = sizeof(monitor_info);
  if (GetMonitorInfo(MonitorFromWindow(nullptr, MONITOR_DEFAULTTOPRIMARY),
                     &monitor_info)) {
    g_primary_monitor_size = gfx::Rect(monitor_info.rcMonitor).size();
  } else {
    g_primary_monitor_size = gfx::Size();
  }
}
}  // namespace

DirectCompositionSurfaceWin::DirectCompositionSurfaceWin(
    HWND parent_window,
    VSyncCallback vsync_callback,
    const Settings& settings)
    : GLSurfaceEGL(),
      child_window_(parent_window),
      root_surface_(new DirectCompositionChildSurfaceWin(
          std::move(vsync_callback),
          settings.use_angle_texture_offset,
          settings.max_pending_frames,
          settings.force_root_surface_full_damage,
          settings.force_root_surface_full_damage_always)),
      layer_tree_(std::make_unique<DCLayerTree>(
          settings.disable_nv12_dynamic_textures,
          settings.disable_vp_scaling,
          settings.no_downscaled_overlay_promotion)) {
  ui::GpuSwitchingManager::GetInstance()->AddObserver(this);
}

DirectCompositionSurfaceWin::~DirectCompositionSurfaceWin() {
  ui::GpuSwitchingManager::GetInstance()->RemoveObserver(this);
  Destroy();
}

// static
void DirectCompositionSurfaceWin::InitializeOneOff() {
  DCHECK(!g_dcomp_device);

  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableDirectComposition))
    return;

  // Direct composition can only be used with ANGLE.
  if (gl::GetGLImplementation() != gl::kGLImplementationEGLANGLE)
    return;

  // Blocklist direct composition if MCTU.dll or MCTUX.dll are injected. These
  // are user mode drivers for display adapters from Magic Control Technology
  // Corporation.
  if (GetModuleHandle(TEXT("MCTU.dll")) || GetModuleHandle(TEXT("MCTUX.dll"))) {
    DLOG(ERROR) << "Blocklisted due to third party modules";
    return;
  }

  // EGL_KHR_no_config_context surface compatibility is required to be able to
  // MakeCurrent with the default pbuffer surface.
  if (!GLSurfaceEGL::IsEGLNoConfigContextSupported()) {
    DLOG(ERROR) << "EGL_KHR_no_config_context not supported";
    return;
  }

  Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device =
      QueryD3D11DeviceObjectFromANGLE();
  if (!d3d11_device) {
    DLOG(ERROR) << "Failed to retrieve D3D11 device";
    return;
  }

  // This will fail if the D3D device is "Microsoft Basic Display Adapter".
  Microsoft::WRL::ComPtr<ID3D11VideoDevice> video_device;
  if (FAILED(d3d11_device.As(&video_device))) {
    DLOG(ERROR) << "Failed to retrieve video device";
    return;
  }

  // Load DLL at runtime since older Windows versions don't have dcomp.
  HMODULE dcomp_module = ::GetModuleHandle(L"dcomp.dll");
  if (!dcomp_module) {
    DLOG(ERROR) << "Failed to load dcomp.dll";
    return;
  }

  using PFN_DCOMPOSITION_CREATE_DEVICE2 = HRESULT(WINAPI*)(
      IUnknown * renderingDevice, REFIID iid, void** dcompositionDevice);
  PFN_DCOMPOSITION_CREATE_DEVICE2 create_device_function =
      reinterpret_cast<PFN_DCOMPOSITION_CREATE_DEVICE2>(
          ::GetProcAddress(dcomp_module, "DCompositionCreateDevice2"));
  if (!create_device_function) {
    DLOG(ERROR) << "GetProcAddress failed for DCompositionCreateDevice2";
    return;
  }

  Microsoft::WRL::ComPtr<IDXGIDevice> dxgi_device;
  d3d11_device.As(&dxgi_device);

  Microsoft::WRL::ComPtr<IDCompositionDesktopDevice> desktop_device;
  HRESULT hr =
      create_device_function(dxgi_device.Get(), IID_PPV_ARGS(&desktop_device));
  if (FAILED(hr)) {
    DLOG(ERROR) << "DCompositionCreateDevice2 failed with error 0x" << std::hex
                << hr;
    return;
  }

  hr = desktop_device.As(&g_dcomp_device);
  if (FAILED(hr)) {
    DLOG(ERROR) << "Failed to retrieve IDCompositionDevice2 with error 0x"
                << std::hex << hr;
    return;
  }
  DCHECK(g_dcomp_device);
}

// static
void DirectCompositionSurfaceWin::ShutdownOneOff() {
  g_dcomp_device.Reset();
}

// static
const Microsoft::WRL::ComPtr<IDCompositionDevice2>&
DirectCompositionSurfaceWin::GetDirectCompositionDevice() {
  return g_dcomp_device;
}

// static
bool DirectCompositionSurfaceWin::IsDirectCompositionSupported() {
  bool swap_chain_failed =
      DirectCompositionChildSurfaceWin::IsDirectCompositionSwapChainFailed();
  return g_dcomp_device && !swap_chain_failed;
}

// static
bool DirectCompositionSurfaceWin::AreOverlaysSupported() {
  // Always initialize and record overlay support information irrespective of
  // command line flags.
  UpdateOverlaySupport();

  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  // Enable flag should be checked before the disable flag, so we could
  // overwrite GPU driver bug workarounds in testing.
  if (command_line->HasSwitch(switches::kEnableDirectCompositionVideoOverlays))
    return true;
  if (command_line->HasSwitch(switches::kDisableDirectCompositionVideoOverlays))
    return false;

  return SupportsOverlays();
}

bool DirectCompositionSurfaceWin::AreHardwareOverlaysSupported() {
  UpdateOverlaySupport();
  return SupportsHardwareOverlays();
}

// static
bool DirectCompositionSurfaceWin::IsDecodeSwapChainSupported() {
  if (!g_decode_swap_chain_disabled) {
    UpdateOverlaySupport();
    return GetOverlayFormatUsedForSDR() == DXGI_FORMAT_NV12;
  }
  return false;
}

// static
void DirectCompositionSurfaceWin::DisableDecodeSwapChain() {
  g_decode_swap_chain_disabled = true;
}

// static
void DirectCompositionSurfaceWin::DisableOverlays() {
  SetSupportsOverlays(false);
  RunOverlayHdrGpuInfoUpdateCallback();
}

// static
void DirectCompositionSurfaceWin::DisableSoftwareOverlays() {
  g_disable_sw_overlays = true;
}

// static
void DirectCompositionSurfaceWin::InvalidateOverlayCaps() {
  SetOverlayCapsValid(false);
}

// static
bool DirectCompositionSurfaceWin::AreScaledOverlaysSupported() {
  UpdateOverlaySupport();
  if (g_overlay_format_used == DXGI_FORMAT_NV12) {
    return (g_nv12_overlay_support_flags & DXGI_OVERLAY_SUPPORT_FLAG_SCALING) ||
           (SupportsOverlays() && SupportsSoftwareOverlays());
  } else if (g_overlay_format_used == DXGI_FORMAT_YUY2) {
    return !!(g_yuy2_overlay_support_flags & DXGI_OVERLAY_SUPPORT_FLAG_SCALING);
  } else {
    DCHECK_EQ(g_overlay_format_used, DXGI_FORMAT_B8G8R8A8_UNORM);
    // Assume scaling is supported for BGRA overlays.
    return true;
  }
}

// static
UINT DirectCompositionSurfaceWin::GetOverlaySupportFlags(DXGI_FORMAT format) {
  UpdateOverlaySupport();
  base::AutoLock auto_lock(GetOverlayLock());
  UINT support_flag = 0;
  switch (format) {
    case DXGI_FORMAT_NV12:
      support_flag = g_nv12_overlay_support_flags;
      break;
    case DXGI_FORMAT_YUY2:
      support_flag = g_yuy2_overlay_support_flags;
      break;
    case DXGI_FORMAT_B8G8R8A8_UNORM:
      support_flag = g_bgra8_overlay_support_flags;
      break;
    case DXGI_FORMAT_R10G10B10A2_UNORM:
      support_flag = g_rgb10a2_overlay_support_flags;
      break;
    default:
      NOTREACHED();
      break;
  }
  return support_flag;
}

// static
gfx::Size DirectCompositionSurfaceWin::GetPrimaryMonitorSize() {
  return g_primary_monitor_size;
}

// static
int DirectCompositionSurfaceWin::GetNumOfMonitors() {
  return g_num_of_monitors;
}

// static
DXGI_FORMAT DirectCompositionSurfaceWin::GetOverlayFormatUsedForSDR() {
  return g_overlay_format_used;
}

// static
void DirectCompositionSurfaceWin::SetScaledOverlaysSupportedForTesting(
    bool supported) {
  UpdateOverlaySupport();
  if (supported) {
    g_nv12_overlay_support_flags |= DXGI_OVERLAY_SUPPORT_FLAG_SCALING;
    g_yuy2_overlay_support_flags |= DXGI_OVERLAY_SUPPORT_FLAG_SCALING;
    g_rgb10a2_overlay_support_flags |= DXGI_OVERLAY_SUPPORT_FLAG_SCALING;
  } else {
    g_nv12_overlay_support_flags &= ~DXGI_OVERLAY_SUPPORT_FLAG_SCALING;
    g_yuy2_overlay_support_flags &= ~DXGI_OVERLAY_SUPPORT_FLAG_SCALING;
    g_rgb10a2_overlay_support_flags &= ~DXGI_OVERLAY_SUPPORT_FLAG_SCALING;
  }
  g_disable_sw_overlays = !supported;
  SetSupportsHardwareOverlays(supported);
  DCHECK_EQ(supported, AreScaledOverlaysSupported());
}

// static
void DirectCompositionSurfaceWin::SetOverlayFormatUsedForTesting(
    DXGI_FORMAT format) {
  DCHECK(format == DXGI_FORMAT_NV12 || format == DXGI_FORMAT_YUY2 ||
         format == DXGI_FORMAT_B8G8R8A8_UNORM);
  UpdateOverlaySupport();
  g_overlay_format_used = format;
  DCHECK_EQ(format, GetOverlayFormatUsedForSDR());
}

// static
bool DirectCompositionSurfaceWin::IsHDRSupported() {
  // HDR support was introduced in Windows 10 Creators Update.
  if (base::win::GetVersion() < base::win::Version::WIN10_RS2)
    return false;

  // Only direct composition surface can allocate HDR swap chains.
  if (!IsDirectCompositionSupported())
    return false;

  HRESULT hr = S_OK;
  Microsoft::WRL::ComPtr<IDXGIFactory1> factory;
  hr = CreateDXGIFactory1(IID_PPV_ARGS(&factory));
  if (FAILED(hr)) {
    DLOG(ERROR) << "Failed to create DXGI factory.";
    return false;
  }

  bool hdr_monitor_found = false;
  for (UINT adapter_index = 0;; ++adapter_index) {
    Microsoft::WRL::ComPtr<IDXGIAdapter> adapter;
    hr = factory->EnumAdapters(adapter_index, &adapter);
    if (hr == DXGI_ERROR_NOT_FOUND)
      break;
    if (FAILED(hr)) {
      DLOG(ERROR) << "Unexpected error creating DXGI adapter.";
      break;
    }

    for (UINT output_index = 0;; ++output_index) {
      Microsoft::WRL::ComPtr<IDXGIOutput> output;
      hr = adapter->EnumOutputs(output_index, &output);
      if (hr == DXGI_ERROR_NOT_FOUND)
        break;
      if (FAILED(hr)) {
        DLOG(ERROR) << "Unexpected error creating DXGI adapter.";
        break;
      }

      Microsoft::WRL::ComPtr<IDXGIOutput6> output6;
      hr = output->QueryInterface(IID_PPV_ARGS(&output6));
      if (FAILED(hr)) {
        DLOG(WARNING) << "IDXGIOutput6 is required for HDR detection.";
        continue;
      }

      DXGI_OUTPUT_DESC1 desc;
      if (FAILED(output6->GetDesc1(&desc))) {
        DLOG(ERROR) << "Unexpected error getting output descriptor.";
        continue;
      }

      if (desc.ColorSpace == DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020) {
        hdr_monitor_found = true;
      }
    }
  }

  UMA_HISTOGRAM_BOOLEAN("GPU.Output.HDR", hdr_monitor_found);
  return hdr_monitor_found;
}

// static
bool DirectCompositionSurfaceWin::IsSwapChainTearingSupported() {
  static const bool supported = [] {
    Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device =
        QueryD3D11DeviceObjectFromANGLE();
    if (!d3d11_device) {
      DLOG(ERROR) << "Not using swap chain tearing because failed to retrieve "
                     "D3D11 device from ANGLE";
      return false;
    }
    Microsoft::WRL::ComPtr<IDXGIDevice> dxgi_device;
    d3d11_device.As(&dxgi_device);
    DCHECK(dxgi_device);
    Microsoft::WRL::ComPtr<IDXGIAdapter> dxgi_adapter;
    dxgi_device->GetAdapter(&dxgi_adapter);
    DCHECK(dxgi_adapter);
    Microsoft::WRL::ComPtr<IDXGIFactory5> dxgi_factory;
    if (FAILED(dxgi_adapter->GetParent(IID_PPV_ARGS(&dxgi_factory)))) {
      DLOG(ERROR) << "Not using swap chain tearing because failed to retrieve "
                     "IDXGIFactory5 interface";
      return false;
    }

    BOOL present_allow_tearing = FALSE;
    DCHECK(dxgi_factory);
    if (FAILED(dxgi_factory->CheckFeatureSupport(
            DXGI_FEATURE_PRESENT_ALLOW_TEARING, &present_allow_tearing,
            sizeof(present_allow_tearing)))) {
      DLOG(ERROR)
          << "Not using swap chain tearing because CheckFeatureSupport failed";
      return false;
    }
    return !!present_allow_tearing;
  }();
  return supported;
}

// static
bool DirectCompositionSurfaceWin::AllowTearing() {
  // Swap chain tearing is used only if vsync is disabled explicitly.
  return !features::UseGpuVsync() &&
         DirectCompositionSurfaceWin::IsSwapChainTearingSupported();
}

// static
void DirectCompositionSurfaceWin::SetOverlayHDRGpuInfoUpdateCallback(
    OverlayHDRInfoUpdateCallback callback) {
  g_overlay_hdr_gpu_info_callback = std::move(callback);
}

// static
void DirectCompositionSurfaceWin::EnableBGRA8OverlaysWithYUVOverlaySupport() {
  // This has to be set before initializing overlay caps.
  DCHECK(!OverlayCapsValid());
  g_enable_bgra8_overlays_with_yuv_overlay_support = true;
}

// static
void DirectCompositionSurfaceWin::ForceNV12OverlaySupport() {
  // This has to be set before initializing overlay caps.
  DCHECK(!OverlayCapsValid());
  g_force_nv12_overlay_support = true;
}

// static
void DirectCompositionSurfaceWin::ForceRgb10a2OverlaySupport() {
  // This has to be set before initializing overlay caps.
  DCHECK(!OverlayCapsValid());
  g_force_rgb10a2_overlay_support = true;
}
// static
void DirectCompositionSurfaceWin::
    SetCheckYCbCrStudioG22LeftP709ForNv12Support() {
  // This has to be set before initializing overlay caps.
  DCHECK(!OverlayCapsValid());
  g_check_ycbcr_studio_g22_left_p709_for_nv12_support = true;
}

bool DirectCompositionSurfaceWin::Initialize(GLSurfaceFormat format) {
  if (!IsDirectCompositionSupported()) {
    DLOG(ERROR) << "Direct composition not supported";
    return false;
  }

  child_window_.Initialize();

  window_ = child_window_.window();

  if (!layer_tree_->Initialize(window_))
    return false;

  if (!root_surface_->Initialize(GLSurfaceFormat()))
    return false;

  UpdateMonitorInfo();
  return true;
}

void DirectCompositionSurfaceWin::Destroy() {
  root_surface_->Destroy();
  // Freeing DComp resources such as visuals and surfaces causes the
  // device to become 'dirty'. We must commit the changes to the device
  // in order for the objects to actually be destroyed.
  // Leaving the device in the dirty state for long periods of time means
  // that if DWM.exe crashes, the Chromium window will become black until
  // the next Commit.
  layer_tree_.reset();
  if (g_dcomp_device)
    g_dcomp_device->Commit();
}

gfx::Size DirectCompositionSurfaceWin::GetSize() {
  return root_surface_->GetSize();
}

bool DirectCompositionSurfaceWin::IsOffscreen() {
  return false;
}

void* DirectCompositionSurfaceWin::GetHandle() {
  return root_surface_->GetHandle();
}

bool DirectCompositionSurfaceWin::Resize(const gfx::Size& size,
                                         float scale_factor,
                                         const gfx::ColorSpace& color_space,
                                         bool has_alpha) {
  // Force a resize and redraw (but not a move, activate, etc.).
  if (!SetWindowPos(window_, nullptr, 0, 0, size.width(), size.height(),
                    SWP_NOMOVE | SWP_NOACTIVATE | SWP_NOCOPYBITS |
                        SWP_NOOWNERZORDER | SWP_NOZORDER)) {
    return false;
  }
  return root_surface_->Resize(size, scale_factor, color_space, has_alpha);
}

gfx::SwapResult DirectCompositionSurfaceWin::SwapBuffers(
    PresentationCallback callback) {
  TRACE_EVENT0("gpu", "DirectCompositionSurfaceWin::SwapBuffers");

  if (root_surface_->SwapBuffers(std::move(callback)) !=
      gfx::SwapResult::SWAP_ACK)
    return gfx::SwapResult::SWAP_FAILED;

  if (!layer_tree_->CommitAndClearPendingOverlays(root_surface_.get()))
    return gfx::SwapResult::SWAP_FAILED;

  return gfx::SwapResult::SWAP_ACK;
}

gfx::SwapResult DirectCompositionSurfaceWin::PostSubBuffer(
    int x,
    int y,
    int width,
    int height,
    PresentationCallback callback) {
  // The arguments are ignored because SetDrawRectangle specified the area to
  // be swapped.
  return SwapBuffers(std::move(callback));
}

gfx::VSyncProvider* DirectCompositionSurfaceWin::GetVSyncProvider() {
  return root_surface_->GetVSyncProvider();
}

void DirectCompositionSurfaceWin::SetVSyncEnabled(bool enabled) {
  root_surface_->SetVSyncEnabled(enabled);
}

bool DirectCompositionSurfaceWin::ScheduleDCLayer(
    std::unique_ptr<ui::DCRendererLayerParams> params) {
  return layer_tree_->ScheduleDCLayer(std::move(params));
}

void DirectCompositionSurfaceWin::SetFrameRate(float frame_rate) {
  // Only try to reduce vsync frequency through the video swap chain.
  // This allows us to experiment UseSetPresentDuration optimization to
  // fullscreen video overlays only and avoid compromising
  // UsePreferredIntervalForVideo optimization where we skip compositing
  // every other frame when fps <= half the vsync frame rate.
  layer_tree_->SetFrameRate(frame_rate);
}

bool DirectCompositionSurfaceWin::SetEnableDCLayers(bool enable) {
  return root_surface_->SetEnableDCLayers(enable);
}

gfx::SurfaceOrigin DirectCompositionSurfaceWin::GetOrigin() const {
  return gfx::SurfaceOrigin::kTopLeft;
}

bool DirectCompositionSurfaceWin::SupportsPostSubBuffer() {
  return true;
}

bool DirectCompositionSurfaceWin::OnMakeCurrent(GLContext* context) {
  return root_surface_->OnMakeCurrent(context);
}

bool DirectCompositionSurfaceWin::SupportsDCLayers() const {
  return true;
}

bool DirectCompositionSurfaceWin::SupportsProtectedVideo() const {
  // TODO(magchen): Check the gpu driver date (or a function) which we know this
  // new support is enabled.
  return AreOverlaysSupported();
}

bool DirectCompositionSurfaceWin::SetDrawRectangle(const gfx::Rect& rectangle) {
  bool result = root_surface_->SetDrawRectangle(rectangle);
  if (!result &&
      DirectCompositionChildSurfaceWin::IsDirectCompositionSwapChainFailed()) {
    RunOverlayHdrGpuInfoUpdateCallback();
  }

  return result;
}

gfx::Vector2d DirectCompositionSurfaceWin::GetDrawOffset() const {
  return root_surface_->GetDrawOffset();
}

bool DirectCompositionSurfaceWin::SupportsGpuVSync() const {
  return true;
}

void DirectCompositionSurfaceWin::SetGpuVSyncEnabled(bool enabled) {
  root_surface_->SetGpuVSyncEnabled(enabled);
}

void DirectCompositionSurfaceWin::OnGpuSwitched(
    gl::GpuPreference active_gpu_heuristic) {}

void DirectCompositionSurfaceWin::OnDisplayAdded() {
  InvalidateOverlayCaps();
  UpdateOverlaySupport();
  UpdateMonitorInfo();
  layer_tree_->GetHDRMetadataHelper()->UpdateDisplayMetadata();
  RunOverlayHdrGpuInfoUpdateCallback();
}

void DirectCompositionSurfaceWin::OnDisplayRemoved() {
  InvalidateOverlayCaps();
  UpdateOverlaySupport();
  UpdateMonitorInfo();
  layer_tree_->GetHDRMetadataHelper()->UpdateDisplayMetadata();
  RunOverlayHdrGpuInfoUpdateCallback();
}

void DirectCompositionSurfaceWin::OnDisplayMetricsChanged() {
  UpdateMonitorInfo();
}

bool DirectCompositionSurfaceWin::SupportsDelegatedInk() {
  return layer_tree_->SupportsDelegatedInk();
}

void DirectCompositionSurfaceWin::SetDelegatedInkTrailStartPoint(
    std::unique_ptr<gfx::DelegatedInkMetadata> metadata) {
  layer_tree_->SetDelegatedInkTrailStartPoint(std::move(metadata));
}

void DirectCompositionSurfaceWin::InitDelegatedInkPointRendererReceiver(
    mojo::PendingReceiver<gfx::mojom::DelegatedInkPointRenderer>
        pending_receiver) {
  layer_tree_->InitDelegatedInkPointRendererReceiver(
      std::move(pending_receiver));
}

scoped_refptr<base::TaskRunner>
DirectCompositionSurfaceWin::GetWindowTaskRunnerForTesting() {
  return child_window_.GetTaskRunnerForTesting();
}

Microsoft::WRL::ComPtr<IDXGISwapChain1>
DirectCompositionSurfaceWin::GetLayerSwapChainForTesting(size_t index) const {
  return layer_tree_->GetLayerSwapChainForTesting(index);
}

Microsoft::WRL::ComPtr<IDXGISwapChain1>
DirectCompositionSurfaceWin::GetBackbufferSwapChainForTesting() const {
  return root_surface_->swap_chain();
}

scoped_refptr<DirectCompositionChildSurfaceWin>
DirectCompositionSurfaceWin::GetRootSurfaceForTesting() const {
  return root_surface_;
}

void DirectCompositionSurfaceWin::GetSwapChainVisualInfoForTesting(
    size_t index,
    gfx::Transform* transform,
    gfx::Point* offset,
    gfx::Rect* clip_rect) const {
  layer_tree_->GetSwapChainVisualInfoForTesting(  // IN-TEST
      index, transform, offset, clip_rect);
}

void DirectCompositionSurfaceWin::SetMonitorInfoForTesting(
    int num_of_monitors,
    gfx::Size monitor_size) {
  g_num_of_monitors = num_of_monitors;
  g_primary_monitor_size = monitor_size;
}

}  // namespace gl
