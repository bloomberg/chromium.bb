// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/win/screen_win.h"

#include <windows.h>
#include <shellscalingapi.h>

#include <algorithm>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/metrics/histogram_functions.h"
#include "base/numerics/ranges.h"
#include "base/optional.h"
#include "base/stl_util.h"
#include "base/win/win_util.h"
#include "base/win/windows_version.h"
#include "ui/display/display.h"
#include "ui/display/display_layout.h"
#include "ui/display/display_layout_builder.h"
#include "ui/display/win/display_info.h"
#include "ui/display/win/dpi.h"
#include "ui/display/win/local_process_window_finder_win.h"
#include "ui/display/win/scaling_util.h"
#include "ui/display/win/screen_win_display.h"
#include "ui/gfx/geometry/point_conversions.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/safe_integer_conversions.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/gfx/icc_profile.h"

namespace display {
namespace win {
namespace {

// TODO(robliao): http://crbug.com/615514 Remove when ScreenWin usage is
// resolved with Desktop Aura and WindowTreeHost.
ScreenWin* g_instance = nullptr;

// Gets the DPI for a particular monitor.
base::Optional<int> GetPerMonitorDPI(HMONITOR monitor) {
  if (!base::win::IsProcessPerMonitorDpiAware())
    return base::nullopt;

  static auto get_dpi_for_monitor_func = []() {
    const HMODULE shcore_dll = ::LoadLibrary(L"shcore.dll");
    return reinterpret_cast<decltype(&::GetDpiForMonitor)>(
        shcore_dll ? ::GetProcAddress(shcore_dll, "GetDpiForMonitor")
                   : nullptr);
  }();
  UINT dpi_x, dpi_y;
  if (!get_dpi_for_monitor_func ||
      !SUCCEEDED(
          get_dpi_for_monitor_func(monitor, MDT_EFFECTIVE_DPI, &dpi_x, &dpi_y)))
    return base::nullopt;

  DCHECK_EQ(dpi_x, dpi_y);
  return int{dpi_x};
}

float GetScaleFactorForDPI(int dpi, bool include_accessibility) {
  const float scale = display::win::internal::GetScalingFactorFromDPI(dpi);
  return include_accessibility
             ? (scale * UwpTextScaleFactor::Instance()->GetTextScaleFactor())
             : scale;
}

// Gets the raw monitor scale factor.
//
// Respects the forced device scale factor, and will fall back to the global
// scale factor if per-monitor DPI is not supported.
float GetMonitorScaleFactor(HMONITOR monitor,
                            bool include_accessibility = true) {
  DCHECK(monitor);
  if (Display::HasForceDeviceScaleFactor())
    return Display::GetForcedDeviceScaleFactor();

  const auto dpi = GetPerMonitorDPI(monitor);
  return dpi ? GetScaleFactorForDPI(dpi.value(), include_accessibility)
             : GetDPIScale();
}

std::vector<DISPLAYCONFIG_PATH_INFO> GetPathInfos() {
  for (LONG result = ERROR_INSUFFICIENT_BUFFER;
       result == ERROR_INSUFFICIENT_BUFFER;) {
    uint32_t path_elements, mode_elements;
    if (GetDisplayConfigBufferSizes(QDC_ONLY_ACTIVE_PATHS, &path_elements,
                                    &mode_elements) != ERROR_SUCCESS) {
      return {};
    }
    std::vector<DISPLAYCONFIG_PATH_INFO> path_infos(path_elements);
    std::vector<DISPLAYCONFIG_MODE_INFO> mode_infos(mode_elements);
    result = QueryDisplayConfig(QDC_ONLY_ACTIVE_PATHS, &path_elements,
                                path_infos.data(), &mode_elements,
                                mode_infos.data(), nullptr);
    if (result == ERROR_SUCCESS) {
      path_infos.resize(path_elements);
      return path_infos;
    }
  }
  return {};
}

base::Optional<DISPLAYCONFIG_PATH_INFO> GetPathInfo(HMONITOR monitor) {
  // Get the monitor name.
  MONITORINFOEX monitor_info = {};
  monitor_info.cbSize = sizeof(monitor_info);
  if (!GetMonitorInfo(monitor, &monitor_info))
    return base::nullopt;

  // Look for a path info with a matching name.
  std::vector<DISPLAYCONFIG_PATH_INFO> path_infos = GetPathInfos();
  for (const auto& info : path_infos) {
    DISPLAYCONFIG_SOURCE_DEVICE_NAME device_name = {};
    device_name.header.type = DISPLAYCONFIG_DEVICE_INFO_GET_SOURCE_NAME;
    device_name.header.size = sizeof(device_name);
    device_name.header.adapterId = info.sourceInfo.adapterId;
    device_name.header.id = info.sourceInfo.id;
    if ((DisplayConfigGetDeviceInfo(&device_name.header) == ERROR_SUCCESS) &&
        (wcscmp(monitor_info.szDevice, device_name.viewGdiDeviceName) == 0))
      return info;
  }
  return base::nullopt;
}

float GetSDRWhiteLevel(const base::Optional<DISPLAYCONFIG_PATH_INFO>& path) {
  if (path) {
    DISPLAYCONFIG_SDR_WHITE_LEVEL white_level = {};
    white_level.header.type = DISPLAYCONFIG_DEVICE_INFO_GET_SDR_WHITE_LEVEL;
    white_level.header.size = sizeof(white_level);
    white_level.header.adapterId = path->targetInfo.adapterId;
    white_level.header.id = path->targetInfo.id;
    if (DisplayConfigGetDeviceInfo(&white_level.header) == ERROR_SUCCESS)
      return white_level.SDRWhiteLevel * 80.0 / 1000.0;
  }
  return 200.0f;
}

DISPLAYCONFIG_VIDEO_OUTPUT_TECHNOLOGY GetOutputTechnology(
    const base::Optional<DISPLAYCONFIG_PATH_INFO>& path) {
  if (path)
    return path->targetInfo.outputTechnology;
  return DISPLAYCONFIG_OUTPUT_TECHNOLOGY_OTHER;
}

// Returns true if |tech| represents an internal display (eg. a laptop screen).
bool IsInternalOutputTechnology(DISPLAYCONFIG_VIDEO_OUTPUT_TECHNOLOGY tech) {
  switch (tech) {
    case DISPLAYCONFIG_OUTPUT_TECHNOLOGY_INTERNAL:
    case DISPLAYCONFIG_OUTPUT_TECHNOLOGY_DISPLAYPORT_EMBEDDED:
    case DISPLAYCONFIG_OUTPUT_TECHNOLOGY_UDI_EMBEDDED:
      return true;

    default:
      return false;
  }
}

Display::Rotation OrientationToRotation(DWORD orientation) {
  switch (orientation) {
    case DMDO_DEFAULT:
      return Display::ROTATE_0;
    case DMDO_90:
      return Display::ROTATE_90;
    case DMDO_180:
      return Display::ROTATE_180;
    case DMDO_270:
      return Display::ROTATE_270;
    default:
      NOTREACHED();
      return Display::ROTATE_0;
  }
}

struct DisplaySettings {
  Display::Rotation rotation;
  int frequency;
};
DisplaySettings GetDisplaySettingsForDevice(const wchar_t* device_name) {
  DEVMODE mode = {};
  mode.dmSize = sizeof(mode);
  if (!::EnumDisplaySettings(device_name, ENUM_CURRENT_SETTINGS, &mode))
    return {Display::ROTATE_0, 0};
  return {OrientationToRotation(mode.dmDisplayOrientation),
          mode.dmDisplayFrequency};
}

std::vector<DisplayInfo> FindAndRemoveTouchingDisplayInfos(
    const DisplayInfo& parent_info,
    std::vector<DisplayInfo>* display_infos) {
  const auto first_touching_it = std::partition(
      display_infos->begin(), display_infos->end(),
      [&](const auto& info) { return !DisplayInfosTouch(parent_info, info); });
  std::vector<DisplayInfo> touching_display_infos(first_touching_it,
                                                  display_infos->end());
  display_infos->erase(first_touching_it, display_infos->end());
  return touching_display_infos;
}

// Default scRGB white level in nits.  This is used to determine the SDR scaling
// factor with the user configured white level from the SDR brightness slider.
constexpr float kDefaultScrgbWhiteLevel = 80.0f;

// Helper function to create gfx::DisplayColorSpaces from given |color_space|
// and |sdr_white_level| with default buffer formats for Windows.
gfx::DisplayColorSpaces CreateDisplayColorSpaces(
    const gfx::ColorSpace& color_space,
    float sdr_white_level = kDefaultScrgbWhiteLevel) {
  gfx::DisplayColorSpaces display_color_spaces(color_space);
  // When alpha is not needed, specify BGRX_8888 to get
  // DXGI_ALPHA_MODE_IGNORE. This saves significant power (see
  // https://crbug.com/1057163).
  display_color_spaces.SetOutputBufferFormats(gfx::BufferFormat::BGRX_8888,
                                              gfx::BufferFormat::BGRA_8888);
  display_color_spaces.SetSDRWhiteLevel(sdr_white_level);
  return display_color_spaces;
}

// Updates |color_spaces| for HDR and WCG content usage with appropriate color
// HDR spaces and given |sdr_white_level|.
gfx::DisplayColorSpaces GetDisplayColorSpacesForHdr(float sdr_white_level) {
  auto color_spaces =
      CreateDisplayColorSpaces(gfx::ColorSpace::CreateSRGB(), sdr_white_level);

  // This will map to DXGI_COLOR_SPACE_RGB_FULL_G10_NONE_P709. In that space,
  // the brightness of (1,1,1) is 80 nits.
  const auto scrgb_linear = gfx::ColorSpace::CreateSCRGBLinear(
      kDefaultScrgbWhiteLevel / sdr_white_level);

  // This will map to DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020, with sRGB's
  // (1,1,1) mapping to the specified number of nits.
  const auto hdr10 = gfx::ColorSpace::CreateHDR10(sdr_white_level);

  // Use HDR color spaces only when there is WCG or HDR content on the screen.
  constexpr bool kNeedsAlpha = true;
  for (const auto& usage : {gfx::ContentColorUsage::kWideColorGamut,
                            gfx::ContentColorUsage::kHDR}) {
    // Using RGBA F16 backbuffers required by SCRGB linear causes stuttering on
    // Windows RS3, but RGB10A2 with HDR10 color space works fine (see
    // https://crbug.com/937108#c92).
    if (base::win::GetVersion() > base::win::Version::WIN10_RS3) {
      color_spaces.SetOutputColorSpaceAndBufferFormat(
          usage, !kNeedsAlpha, scrgb_linear, gfx::BufferFormat::RGBA_F16);
    } else {
      color_spaces.SetOutputColorSpaceAndBufferFormat(
          usage, !kNeedsAlpha, hdr10, gfx::BufferFormat::RGBA_1010102);
    }
    // Use RGBA F16 backbuffers for HDR if alpha channel is required.
    color_spaces.SetOutputColorSpaceAndBufferFormat(
        usage, kNeedsAlpha, scrgb_linear, gfx::BufferFormat::RGBA_F16);
  }
  return color_spaces;
}

// Sets SDR white level and buffer formats on |display_color_spaces| when using
// a forced color profile.
gfx::DisplayColorSpaces GetForcedDisplayColorSpaces() {
  // Adjust white level to default for Windows if color space is PQ.  This is
  // needed because the color space is created with the cross-platform default
  // white level which is different (gfx::ColorSpace::kDefaultSDRWhiteLevel).
  const auto& color_space =
      Display::GetForcedDisplayColorProfile().GetWithPQSDRWhiteLevel(
          kDefaultScrgbWhiteLevel);
  auto display_color_spaces = CreateDisplayColorSpaces(color_space);
  // Use the forced color profile's buffer format for all content usages.
  if (color_space.GetTransferID() == gfx::ColorSpace::TransferID::SMPTEST2084) {
    display_color_spaces.SetOutputBufferFormats(
        gfx::BufferFormat::RGBA_1010102, gfx::BufferFormat::RGBA_1010102);
  } else if (color_space.IsHDR()) {
    display_color_spaces.SetOutputBufferFormats(gfx::BufferFormat::RGBA_F16,
                                                gfx::BufferFormat::RGBA_F16);
  }
  return display_color_spaces;
}

Display CreateDisplayFromDisplayInfo(
    const DisplayInfo& display_info,
    const ColorProfileReader* color_profile_reader,
    bool hdr_enabled) {
  const float scale_factor = display_info.device_scale_factor();
  const gfx::Rect bounds = gfx::ScaleToEnclosingRect(display_info.screen_rect(),
                                                     1.0f / scale_factor);
  Display display(display_info.id(), bounds);
  display.set_device_scale_factor(scale_factor);
  display.set_work_area(gfx::ScaleToEnclosingRect(
      display_info.screen_work_rect(), 1.0f / scale_factor));
  display.set_rotation(display_info.rotation());
  display.set_display_frequency(display_info.display_frequency());

  // DisplayColorSpaces is created using the forced color profile if present, or
  // from the ICC profile provided by |color_profile_reader| for SDR content,
  // and HDR10 or scRGB linear for HDR and WCG content if HDR is enabled.
  gfx::DisplayColorSpaces color_spaces;
  if (Display::HasForceDisplayColorProfile()) {
    color_spaces = GetForcedDisplayColorSpaces();
  } else if (hdr_enabled) {
    color_spaces = GetDisplayColorSpacesForHdr(display_info.sdr_white_level());
  } else {
    color_spaces = CreateDisplayColorSpaces(
        color_profile_reader->GetDisplayColorSpace(display.id()));
  }
  if (color_spaces.SupportsHDR()) {
    // These are (ab)used by pages via media query APIs to detect HDR support.
    display.set_color_depth(Display::kHDR10BitsPerPixel);
    display.set_depth_per_component(Display::kHDR10BitsPerComponent);
  }
  display.set_color_spaces(color_spaces);
  return display;
}

// Windows historically has had a hard time handling displays of DPIs higher
// than 96. Handling multiple DPI displays means we have to deal with Windows'
// monitor physical coordinates and map into Chrome's DIP coordinates.
//
// To do this, DisplayInfosToScreenWinDisplays reasons over monitors as a tree
// using the primary monitor as the root. All monitors touching this root are
// considered children.
//
// This also presumes that all monitors are connected components. By UI
// construction, Windows restricts the layout of monitors to connected
// components except when DPI virtualization is happening. When this happens, we
// scale relative to (0, 0).
//
// Note that this does not handle cases where a scaled display may have
// insufficient room to lay out its children. In these cases, a DIP point could
// map to multiple screen points due to overlap. The first discovered screen
// will take precedence.
std::vector<ScreenWinDisplay> DisplayInfosToScreenWinDisplays(
    const std::vector<DisplayInfo>& display_infos,
    ColorProfileReader* color_profile_reader,
    bool hdr_enabled) {
  // Find and extract the primary display.
  std::vector<DisplayInfo> display_infos_remaining = display_infos;
  auto primary_display_iter = std::find_if(
      display_infos_remaining.begin(), display_infos_remaining.end(),
      [](const DisplayInfo& display_info) {
        return display_info.screen_rect().origin().IsOrigin();
      });
  DCHECK(primary_display_iter != display_infos_remaining.end());

  // Build the tree and determine DisplayPlacements along the way.
  DisplayLayoutBuilder builder(primary_display_iter->id());
  std::vector<DisplayInfo> available_parents = {*primary_display_iter};
  display_infos_remaining.erase(primary_display_iter);
  while (!available_parents.empty()) {
    const DisplayInfo parent = available_parents.back();
    available_parents.pop_back();
    for (const auto& child :
         FindAndRemoveTouchingDisplayInfos(parent, &display_infos_remaining)) {
      builder.AddDisplayPlacement(CalculateDisplayPlacement(parent, child));
      available_parents.push_back(child);
    }
  }

  // Layout and create the ScreenWinDisplays.
  std::vector<Display> displays;
  for (const auto& display_info : display_infos) {
    displays.push_back(CreateDisplayFromDisplayInfo(
        display_info, color_profile_reader, hdr_enabled));
  }
  builder.Build()->ApplyToDisplayList(&displays, nullptr, 0);

  std::vector<ScreenWinDisplay> screen_win_displays;
  for (size_t i = 0; i < display_infos.size(); ++i)
    screen_win_displays.emplace_back(displays[i], display_infos[i]);
  return screen_win_displays;
}

std::vector<Display> ScreenWinDisplaysToDisplays(
    const std::vector<ScreenWinDisplay>& screen_win_displays) {
  std::vector<Display> displays;
  for (const auto& screen_win_display : screen_win_displays)
    displays.push_back(screen_win_display.display());
  return displays;
}

MONITORINFOEX MonitorInfoFromHMONITOR(HMONITOR monitor) {
  MONITORINFOEX monitor_info = {};
  monitor_info.cbSize = sizeof(monitor_info);
  ::GetMonitorInfo(monitor, &monitor_info);
  return monitor_info;
}

base::Optional<gfx::Vector2dF> GetPixelsPerInchForPointerDevice(
    HANDLE source_device) {
  static const auto get_pointer_device_rects =
      reinterpret_cast<decltype(&::GetPointerDeviceRects)>(
          base::win::GetUser32FunctionPointer("GetPointerDeviceRects"));
  RECT device_rect, screen_rect;
  if (!get_pointer_device_rects ||
      !get_pointer_device_rects(source_device, &device_rect, &screen_rect))
    return base::nullopt;

  const gfx::RectF device{gfx::Rect(device_rect)};
  const gfx::RectF screen{gfx::Rect(screen_rect)};
  constexpr float kHimetricPerInch = 2540.0f;
  const float himetric_per_pixel_x = device.width() / screen.width();
  const float himetric_per_pixel_y = device.height() / screen.height();
  return gfx::Vector2dF(kHimetricPerInch / himetric_per_pixel_x,
                        kHimetricPerInch / himetric_per_pixel_y);
}

// Returns physical pixels per inch based on 96 dpi monitor.
gfx::Vector2dF GetDefaultMonitorPhysicalPixelsPerInch() {
  const int default_dpi = GetDPIFromScalingFactor(1.0f);
  return gfx::Vector2dF(default_dpi, default_dpi);
}

// Retrieves PPI for |monitor| based on touch pointer device handles.  Returns
// nullopt if a pointer device for |monitor| can't be found.
base::Optional<gfx::Vector2dF> GetMonitorPixelsPerInch(HMONITOR monitor) {
  static const auto get_pointer_devices =
      reinterpret_cast<decltype(&::GetPointerDevices)>(
          base::win::GetUser32FunctionPointer("GetPointerDevices"));
  uint32_t pointer_device_count = 0;
  if (!get_pointer_devices ||
      !get_pointer_devices(&pointer_device_count, nullptr) ||
      (pointer_device_count == 0))
    return base::nullopt;

  std::vector<POINTER_DEVICE_INFO> pointer_devices(pointer_device_count);
  if (!get_pointer_devices(&pointer_device_count, pointer_devices.data()))
    return base::nullopt;

  for (const auto& device : pointer_devices) {
    if (device.pointerDeviceType == POINTER_DEVICE_TYPE_TOUCH &&
        device.monitor == monitor)
      return GetPixelsPerInchForPointerDevice(device.device);
  }
  return base::nullopt;
}

BOOL CALLBACK EnumMonitorForDisplayInfoCallback(HMONITOR monitor,
                                                HDC hdc,
                                                LPRECT rect,
                                                LPARAM data) {
  const MONITORINFOEX monitor_info = MonitorInfoFromHMONITOR(monitor);
  const auto display_settings =
      GetDisplaySettingsForDevice(monitor_info.szDevice);
  const gfx::Vector2dF pixels_per_inch =
      GetMonitorPixelsPerInch(monitor).value_or(
          GetDefaultMonitorPhysicalPixelsPerInch());
  const auto path_info = GetPathInfo(monitor);

  auto* display_infos = reinterpret_cast<std::vector<DisplayInfo>*>(data);
  DCHECK(display_infos);
  display_infos->emplace_back(
      monitor_info, GetMonitorScaleFactor(monitor), GetSDRWhiteLevel(path_info),
      display_settings.rotation, display_settings.frequency, pixels_per_inch,
      GetOutputTechnology(path_info));
  return TRUE;
}

std::vector<DisplayInfo> GetDisplayInfosFromSystem() {
  std::vector<DisplayInfo> display_infos;
  EnumDisplayMonitors(nullptr, nullptr, EnumMonitorForDisplayInfoCallback,
                      reinterpret_cast<LPARAM>(&display_infos));
  DCHECK_EQ(::GetSystemMetrics(SM_CMONITORS), int{display_infos.size()});
  return display_infos;
}

// Returns |point|, transformed from |from_origin|'s to |to_origin|'s
// coordinates, which differ by |scale_factor|.
gfx::PointF ScalePointRelative(const gfx::PointF& point,
                               const gfx::Point& from_origin,
                               const gfx::Point& to_origin,
                               const float scale_factor) {
  const gfx::PointF relative_point = point - from_origin.OffsetFromOrigin();
  const gfx::PointF scaled_relative_point =
      gfx::ScalePoint(relative_point, scale_factor);
  return scaled_relative_point + to_origin.OffsetFromOrigin();
}

gfx::PointF ScreenToDIPPoint(const gfx::PointF& screen_point,
                             const ScreenWinDisplay& screen_win_display) {
  const Display display = screen_win_display.display();
  return ScalePointRelative(
      screen_point, screen_win_display.pixel_bounds().origin(),
      display.bounds().origin(), 1.0f / display.device_scale_factor());
}

gfx::Point DIPToScreenPoint(const gfx::Point& dip_point,
                            const ScreenWinDisplay& screen_win_display) {
  const Display display = screen_win_display.display();
  return gfx::ToFlooredPoint(
      ScalePointRelative(gfx::PointF(dip_point), display.bounds().origin(),
                         screen_win_display.pixel_bounds().origin(),
                         display.device_scale_factor()));
}

}  // namespace

ScreenWin::ScreenWin() : ScreenWin(true) {}

ScreenWin::~ScreenWin() {
  DCHECK_EQ(g_instance, this);
  g_instance = nullptr;
}

// static
gfx::PointF ScreenWin::ScreenToDIPPoint(const gfx::PointF& pixel_point) {
  const ScreenWinDisplay screen_win_display =
      GetScreenWinDisplayVia(&ScreenWin::GetScreenWinDisplayNearestScreenPoint,
                             gfx::ToFlooredPoint(pixel_point));
  return display::win::ScreenToDIPPoint(pixel_point, screen_win_display);
}

// static
gfx::Point ScreenWin::DIPToScreenPoint(const gfx::Point& dip_point) {
  const ScreenWinDisplay screen_win_display = GetScreenWinDisplayVia(
      &ScreenWin::GetScreenWinDisplayNearestDIPPoint, dip_point);
  return display::win::DIPToScreenPoint(dip_point, screen_win_display);
}

// static
gfx::Point ScreenWin::ClientToDIPPoint(HWND hwnd,
                                       const gfx::Point& client_point) {
  return ScaleToFlooredPoint(client_point, 1.0f / GetScaleFactorForHWND(hwnd));
}

// static
gfx::Point ScreenWin::DIPToClientPoint(HWND hwnd, const gfx::Point& dip_point) {
  return ScaleToFlooredPoint(dip_point, GetScaleFactorForHWND(hwnd));
}

// static
gfx::Rect ScreenWin::ScreenToDIPRect(HWND hwnd, const gfx::Rect& pixel_bounds) {
  const ScreenWinDisplay screen_win_display = hwnd
      ? GetScreenWinDisplayVia(&ScreenWin::GetScreenWinDisplayNearestHWND, hwnd)
      : GetScreenWinDisplayVia(
            &ScreenWin::GetScreenWinDisplayNearestScreenRect, pixel_bounds);
  const gfx::Point origin = gfx::ToFlooredPoint(display::win::ScreenToDIPPoint(
      gfx::PointF(pixel_bounds.origin()), screen_win_display));
  const float scale_factor =
      1.0f / screen_win_display.display().device_scale_factor();
  return {origin, ScaleToEnclosingRect(pixel_bounds, scale_factor).size()};
}

// static
gfx::Rect ScreenWin::DIPToScreenRect(HWND hwnd, const gfx::Rect& dip_bounds) {
  const ScreenWinDisplay screen_win_display = hwnd
      ? GetScreenWinDisplayVia(&ScreenWin::GetScreenWinDisplayNearestHWND, hwnd)
      : GetScreenWinDisplayVia(
            &ScreenWin::GetScreenWinDisplayNearestDIPRect, dip_bounds);
  const gfx::Point origin =
      display::win::DIPToScreenPoint(dip_bounds.origin(), screen_win_display);
  const float scale_factor = screen_win_display.display().device_scale_factor();
  return {origin, ScaleToEnclosingRect(dip_bounds, scale_factor).size()};
}

// static
gfx::Rect ScreenWin::ClientToDIPRect(HWND hwnd, const gfx::Rect& pixel_bounds) {
  return ScaleToEnclosingRect(pixel_bounds, 1.0f / GetScaleFactorForHWND(hwnd));
}

// static
gfx::Rect ScreenWin::DIPToClientRect(HWND hwnd, const gfx::Rect& dip_bounds) {
  return ScaleToEnclosingRect(dip_bounds, GetScaleFactorForHWND(hwnd));
}

// static
gfx::Size ScreenWin::ScreenToDIPSize(HWND hwnd,
                                     const gfx::Size& size_in_pixels) {
  // Always ceil sizes. Otherwise we may be leaving off part of the bounds.
  return ScaleToCeiledSize(size_in_pixels, 1.0f / GetScaleFactorForHWND(hwnd));
}

// static
gfx::Size ScreenWin::DIPToScreenSize(HWND hwnd, const gfx::Size& dip_size) {
  // Always ceil sizes. Otherwise we may be leaving off part of the bounds.
  return ScaleToCeiledSize(dip_size, GetScaleFactorForHWND(hwnd));
}

// static
int ScreenWin::GetSystemMetricsForMonitor(HMONITOR monitor, int metric) {
  if (!g_instance)
    return ::GetSystemMetrics(metric);

  // Fall back to the primary display's HMONITOR.
  if (!monitor)
    monitor = MonitorFromWindow(nullptr, MONITOR_DEFAULTTOPRIMARY);

  // We don't include fudge factors stemming from accessiblility features when
  // dealing with system metrics associated with window elements drawn by the
  // operating system, since we will not be doing scaling of those metrics
  // ourselves.
  const bool include_accessibility = (metric != SM_CXSIZEFRAME) &&
                                     (metric != SM_CYSIZEFRAME) &&
                                     (metric != SM_CXPADDEDBORDER);

  // We'll then pull up the system metrics scaled by the appropriate amount.
  return g_instance->GetSystemMetricsForScaleFactor(
      GetMonitorScaleFactor(monitor, include_accessibility), metric);
}

// static
int ScreenWin::GetSystemMetricsInDIP(int metric) {
  return g_instance ? g_instance->GetSystemMetricsForScaleFactor(1.0f, metric)
                    : ::GetSystemMetrics(metric);
}

// static
float ScreenWin::GetScaleFactorForHWND(HWND hwnd) {
  const HWND root_hwnd = g_instance ? g_instance->GetRootWindow(hwnd) : hwnd;
  const ScreenWinDisplay screen_win_display = GetScreenWinDisplayVia(
      &ScreenWin::GetScreenWinDisplayNearestHWND, root_hwnd);
  return screen_win_display.display().device_scale_factor();
}

// static
gfx::Vector2dF ScreenWin::GetPixelsPerInch(const gfx::PointF& point) {
  const ScreenWinDisplay screen_win_display =
      GetScreenWinDisplayVia(&ScreenWin::GetScreenWinDisplayNearestDIPPoint,
                             gfx::ToFlooredPoint(point));
  return screen_win_display.pixels_per_inch();
}

// static
int ScreenWin::GetDPIForHWND(HWND hwnd) {
  if (Display::HasForceDeviceScaleFactor())
    return GetDPIFromScalingFactor(Display::GetForcedDeviceScaleFactor());

  const HMONITOR monitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
  return GetPerMonitorDPI(monitor).value_or(
      display::win::internal::GetDefaultSystemDPI());
}

// static
float ScreenWin::GetScaleFactorForDPI(int dpi) {
  return display::win::GetScaleFactorForDPI(dpi, true);
}

// static
float ScreenWin::GetSystemScaleFactor() {
  return display::win::internal::GetUnforcedDeviceScaleFactor();
}

// static
void ScreenWin::SetRequestHDRStatusCallback(
    RequestHDRStatusCallback request_hdr_status_callback) {
  if (g_instance) {
    g_instance->request_hdr_status_callback_ =
        std::move(request_hdr_status_callback);
    g_instance->request_hdr_status_callback_.Run();
  }
}

// static
void ScreenWin::SetHDREnabled(bool hdr_enabled) {
  if (g_instance && (g_instance->hdr_enabled_ != hdr_enabled)) {
    g_instance->hdr_enabled_ = hdr_enabled;
    g_instance->UpdateAllDisplaysAndNotify();
  }
}

HWND ScreenWin::GetHWNDFromNativeWindow(gfx::NativeWindow window) const {
  NOTREACHED();
  return nullptr;
}

gfx::NativeWindow ScreenWin::GetNativeWindowFromHWND(HWND hwnd) const {
  NOTREACHED();
  return nullptr;
}

ScreenWin::ScreenWin(bool initialize) {
  DCHECK(!g_instance);
  g_instance = this;
  if (initialize)
    Initialize();
}

gfx::Point ScreenWin::GetCursorScreenPoint() {
  POINT pt;
  ::GetCursorPos(&pt);
  return gfx::ToFlooredPoint(ScreenToDIPPoint(gfx::PointF(gfx::Point(pt))));
}

bool ScreenWin::IsWindowUnderCursor(gfx::NativeWindow window) {
  POINT cursor_loc;
  return ::GetCursorPos(&cursor_loc) &&
         (GetNativeWindowFromHWND(::WindowFromPoint(cursor_loc)) == window);
}

gfx::NativeWindow ScreenWin::GetWindowAtScreenPoint(const gfx::Point& point) {
  const gfx::Point screen_point = DIPToScreenPoint(point);
  return GetNativeWindowFromHWND(WindowFromPoint(screen_point.ToPOINT()));
}

gfx::NativeWindow ScreenWin::GetLocalProcessWindowAtPoint(
    const gfx::Point& point,
    const std::set<gfx::NativeWindow>& ignore) {
  std::set<HWND> hwnd_set;
  for (auto* const window : ignore) {
    HWND w = GetHWNDFromNativeWindow(window);
    if (w)
      hwnd_set.emplace(w);
  }

  return LocalProcessWindowFinder::GetProcessWindowAtPoint(point, hwnd_set,
                                                           this);
}

int ScreenWin::GetNumDisplays() const {
  return int{screen_win_displays_.size()};
}

const std::vector<Display>& ScreenWin::GetAllDisplays() const {
  return displays_;
}

Display ScreenWin::GetDisplayNearestWindow(gfx::NativeWindow window) const {
  const HWND window_hwnd = window ? GetHWNDFromNativeWindow(window) : nullptr;
  // When |window| isn't rooted to a display, we should just return the default
  // display so we get some correct display information like the scaling factor.
  return window_hwnd ? GetScreenWinDisplayNearestHWND(window_hwnd).display()
                     : GetPrimaryDisplay();
}

Display ScreenWin::GetDisplayNearestPoint(const gfx::Point& point) const {
  const gfx::Point screen_point = DIPToScreenPoint(point);
  return GetScreenWinDisplayNearestScreenPoint(screen_point).display();
}

Display ScreenWin::GetDisplayMatching(const gfx::Rect& match_rect) const {
  return GetScreenWinDisplayNearestScreenRect(match_rect).display();
}

Display ScreenWin::GetPrimaryDisplay() const {
  return GetPrimaryScreenWinDisplay().display();
}

void ScreenWin::AddObserver(DisplayObserver* observer) {
  change_notifier_.AddObserver(observer);
}

void ScreenWin::RemoveObserver(DisplayObserver* observer) {
  change_notifier_.RemoveObserver(observer);
}

gfx::Rect ScreenWin::ScreenToDIPRectInWindow(
    gfx::NativeWindow window,
    const gfx::Rect& screen_rect) const {
  const HWND hwnd = window ? GetHWNDFromNativeWindow(window) : nullptr;
  return ScreenToDIPRect(hwnd, screen_rect);
}

gfx::Rect ScreenWin::DIPToScreenRectInWindow(gfx::NativeWindow window,
                                             const gfx::Rect& dip_rect) const {
  const HWND hwnd = window ? GetHWNDFromNativeWindow(window) : nullptr;
  return DIPToScreenRect(hwnd, dip_rect);
}

void ScreenWin::UpdateFromDisplayInfos(
    const std::vector<DisplayInfo>& display_infos) {
  screen_win_displays_ = DisplayInfosToScreenWinDisplays(
      display_infos, color_profile_reader_.get(), hdr_enabled_);
  displays_ = ScreenWinDisplaysToDisplays(screen_win_displays_);
  for (const auto& display_info : display_infos) {
    if (IsInternalOutputTechnology(display_info.output_technology())) {
      // TODO(crbug.com/1078903): Support multiple internal displays.
      Display::SetInternalDisplayId(display_info.id());
      break;
    }
  }
}

void ScreenWin::Initialize() {
  color_profile_reader_->UpdateIfNeeded();
  singleton_hwnd_observer_ = std::make_unique<gfx::SingletonHwndObserver>(
      base::BindRepeating(&ScreenWin::OnWndProc, base::Unretained(this)));
  UpdateFromDisplayInfos(GetDisplayInfosFromSystem());
  RecordDisplayScaleFactors();

  // We want to remember that we've observed a screen metrics object so that we
  // can remove ourselves as an observer at some later point (either when the
  // metrics object notifies us it's going away or when we are destructed).
  scale_factor_observer_.Add(UwpTextScaleFactor::Instance());
}

MONITORINFOEX ScreenWin::MonitorInfoFromScreenPoint(
    const gfx::Point& screen_point) const {
  return MonitorInfoFromHMONITOR(
      ::MonitorFromPoint(screen_point.ToPOINT(), MONITOR_DEFAULTTONEAREST));
}

MONITORINFOEX ScreenWin::MonitorInfoFromScreenRect(const gfx::Rect& screen_rect)
    const {
  const RECT win_rect = screen_rect.ToRECT();
  return MonitorInfoFromHMONITOR(
      ::MonitorFromRect(&win_rect, MONITOR_DEFAULTTONEAREST));
}

MONITORINFOEX ScreenWin::MonitorInfoFromWindow(HWND hwnd,
                                               DWORD default_options) const {
  return MonitorInfoFromHMONITOR(::MonitorFromWindow(hwnd, default_options));
}

HWND ScreenWin::GetRootWindow(HWND hwnd) const {
  return ::GetAncestor(hwnd, GA_ROOT);
}

int ScreenWin::GetSystemMetrics(int metric) const {
  return ::GetSystemMetrics(metric);
}

void ScreenWin::OnWndProc(HWND hwnd,
                          UINT message,
                          WPARAM wparam,
                          LPARAM lparam) {
  if (message != WM_DISPLAYCHANGE &&
      (message != WM_ACTIVATEAPP || wparam != TRUE) &&
      (message != WM_SETTINGCHANGE || wparam != SPI_SETWORKAREA))
    return;

  color_profile_reader_->UpdateIfNeeded();
  if (request_hdr_status_callback_)
    request_hdr_status_callback_.Run();
  UpdateAllDisplaysAndNotify();
}

void ScreenWin::OnColorProfilesChanged() {
  // The color profile reader will often just confirm that our guess that the
  // color profile was sRGB was indeed correct. Avoid doing an update in these
  // cases.
  if (std::any_of(
          displays_.cbegin(), displays_.cend(), [this](const auto& display) {
            return display.color_spaces().GetRasterColorSpace() !=
                   color_profile_reader_->GetDisplayColorSpace(display.id());
          }))
    UpdateAllDisplaysAndNotify();
}

void ScreenWin::UpdateAllDisplaysAndNotify() {
  std::vector<Display> old_displays = std::move(displays_);
  UpdateFromDisplayInfos(GetDisplayInfosFromSystem());
  change_notifier_.NotifyDisplaysChanged(old_displays, displays_);
}

ScreenWinDisplay ScreenWin::GetScreenWinDisplayNearestHWND(HWND hwnd) const {
  return GetScreenWinDisplay(MonitorInfoFromWindow(hwnd,
                                                   MONITOR_DEFAULTTONEAREST));
}

ScreenWinDisplay ScreenWin::GetScreenWinDisplayNearestScreenRect(
    const gfx::Rect& screen_rect) const {
  return GetScreenWinDisplay(MonitorInfoFromScreenRect(screen_rect));
}

ScreenWinDisplay ScreenWin::GetScreenWinDisplayNearestScreenPoint(
    const gfx::Point& screen_point) const {
  return GetScreenWinDisplay(MonitorInfoFromScreenPoint(screen_point));
}

ScreenWinDisplay ScreenWin::GetScreenWinDisplayNearestDIPPoint(
    const gfx::Point& dip_point) const {
  ScreenWinDisplay primary_screen_win_display;
  for (const auto& screen_win_display : screen_win_displays_) {
    const gfx::Rect dip_bounds = screen_win_display.display().bounds();
    if (dip_bounds.Contains(dip_point))
      return screen_win_display;
    if (dip_bounds.origin().IsOrigin())
      primary_screen_win_display = screen_win_display;
  }
  return primary_screen_win_display;
}

ScreenWinDisplay ScreenWin::GetScreenWinDisplayNearestDIPRect(
    const gfx::Rect& dip_rect) const {
  const auto first_closer = [dip_rect](const auto& display1,
                                       const auto& display2) {
    return SquaredDistanceBetweenRects(dip_rect, display1.display().bounds()) <
           SquaredDistanceBetweenRects(dip_rect, display2.display().bounds());
  };
  const auto it = std::min_element(screen_win_displays_.cbegin(),
                                   screen_win_displays_.cend(), first_closer);
  return (it == screen_win_displays_.cend()) ? ScreenWinDisplay() : *it;
}

ScreenWinDisplay ScreenWin::GetPrimaryScreenWinDisplay() const {
  const ScreenWinDisplay screen_win_display = GetScreenWinDisplay(
      MonitorInfoFromWindow(nullptr, MONITOR_DEFAULTTOPRIMARY));
  // The Windows primary monitor is defined to have an origin of (0, 0).
  DCHECK(screen_win_display.display().bounds().origin().IsOrigin());
  return screen_win_display;
}

ScreenWinDisplay ScreenWin::GetScreenWinDisplay(
    const MONITORINFOEX& monitor_info) const {
  const int64_t id = DisplayInfo::DeviceIdFromDeviceName(monitor_info.szDevice);
  const auto it = std::find_if(
      screen_win_displays_.cbegin(), screen_win_displays_.cend(),
      [id](const auto& display) { return display.display().id() == id; });
  // There is 1:1 correspondence between MONITORINFOEX and ScreenWinDisplay.
  // If we found no screens, either there are no screens, or we're in the midst
  // of updating our screens (see crbug.com/768845); either way, hand out the
  // default display.
  return (it == screen_win_displays_.cend()) ? ScreenWinDisplay() : *it;
}

// static
template <typename Getter, typename GetterType>
ScreenWinDisplay ScreenWin::GetScreenWinDisplayVia(Getter getter,
                                                   GetterType value) {
  return g_instance ? (g_instance->*getter)(value) : ScreenWinDisplay();
}

int ScreenWin::GetSystemMetricsForScaleFactor(float scale_factor,
                                              int metric) const {
  if (base::win::IsProcessPerMonitorDpiAware()) {
    static const auto get_system_metrics_for_dpi =
        reinterpret_cast<decltype(&::GetSystemMetricsForDpi)>(
            base::win::GetUser32FunctionPointer("GetSystemMetricsForDpi"));
    if (get_system_metrics_for_dpi) {
      return get_system_metrics_for_dpi(metric,
                                        GetDPIFromScalingFactor(scale_factor));
    }
  }

  // Windows 8.1 doesn't support GetSystemMetricsForDpi(), yet does support
  // per-process dpi awareness.
  return gfx::ToRoundedInt(GetSystemMetrics(metric) * scale_factor /
                           GetPrimaryDisplay().device_scale_factor());
}

void ScreenWin::RecordDisplayScaleFactors() const {
  std::vector<int> unique_scale_factors;
  for (const auto& screen_win_display : screen_win_displays_) {
    const float scale_factor =
        screen_win_display.display().device_scale_factor();
    // Multiply the reported value by 100 to display it as a percentage. Clamp
    // it so that if it's wildly out-of-band we won't send it to the backend.
    const int reported_scale = base::ClampToRange(
        base::checked_cast<int>(scale_factor * 100), 0, 1000);
    if (!base::Contains(unique_scale_factors, reported_scale)) {
      unique_scale_factors.push_back(reported_scale);
      base::UmaHistogramSparse("UI.DeviceScale", reported_scale);
    }
  }
}

void ScreenWin::OnUwpTextScaleFactorChanged() {
  UpdateAllDisplaysAndNotify();
}

void ScreenWin::OnUwpTextScaleFactorCleanup(UwpTextScaleFactor* source) {
  if (scale_factor_observer_.IsObserving(source))
    scale_factor_observer_.Remove(source);
  UwpTextScaleFactor::Observer::OnUwpTextScaleFactorCleanup(source);
}

}  // namespace win
}  // namespace display
