// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/desktop_resizer.h"

#include <windows.h>

#include <map>

#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"

namespace {
// TODO(jamiewalch): Use the correct DPI for the mode: http://crbug.com/172405.
const int kDefaultDPI = 96;
}  // namespace

namespace remoting {

// Provide comparison operation for ScreenResolution so we can use it in
// std::map.
static inline bool operator <(const ScreenResolution& a,
                              const ScreenResolution& b) {
  if (a.dimensions().width() != b.dimensions().width())
    return a.dimensions().width() < b.dimensions().width();
  if (a.dimensions().height() != b.dimensions().height())
    return a.dimensions().height() < b.dimensions().height();
  if (a.dpi().x() != b.dpi().x())
    return a.dpi().x() < b.dpi().x();
  return a.dpi().y() < b.dpi().y();
}

class DesktopResizerWin : public DesktopResizer {
 public:
  DesktopResizerWin();
  ~DesktopResizerWin() override;

  // DesktopResizer interface.
  ScreenResolution GetCurrentResolution() override;
  std::list<ScreenResolution> GetSupportedResolutions(
      const ScreenResolution& preferred) override;
  void SetResolution(const ScreenResolution& resolution) override;
  void RestoreResolution(const ScreenResolution& original) override;

 private:
  static bool IsResizeSupported();

  // Calls EnumDisplaySettingsEx() for the primary monitor.
  // Returns false if |mode_number| does not exist.
  static bool GetPrimaryDisplayMode(
      DWORD mode_number, DWORD flags, DEVMODE* mode);

  // Returns true if the mode has width, height, bits-per-pixel, frequency
  // and orientation fields.
  static bool IsModeValid(const DEVMODE& mode);

  // Returns the width & height of |mode|, or 0x0 if they are missing.
  static ScreenResolution GetModeResolution(const DEVMODE& mode);

  std::map<ScreenResolution, DEVMODE> best_mode_for_resolution_;

  DISALLOW_COPY_AND_ASSIGN(DesktopResizerWin);
};

DesktopResizerWin::DesktopResizerWin() {
}

DesktopResizerWin::~DesktopResizerWin() {
}

ScreenResolution DesktopResizerWin::GetCurrentResolution() {
  DEVMODE current_mode;
  if (GetPrimaryDisplayMode(ENUM_CURRENT_SETTINGS, 0, &current_mode) &&
      IsModeValid(current_mode))
    return GetModeResolution(current_mode);
  return ScreenResolution();
}

std::list<ScreenResolution> DesktopResizerWin::GetSupportedResolutions(
    const ScreenResolution& preferred) {
  if (!IsResizeSupported())
    return std::list<ScreenResolution>();

  // Enumerate the resolutions to return, and where there are multiple modes of
  // the same resolution, store the one most closely matching the current mode
  // in |best_mode_for_resolution_|.
  DEVMODE current_mode;
  if (!GetPrimaryDisplayMode(ENUM_CURRENT_SETTINGS, 0, &current_mode) ||
      !IsModeValid(current_mode))
    return std::list<ScreenResolution>();

  std::list<ScreenResolution> resolutions;
  best_mode_for_resolution_.clear();
  for (DWORD i = 0; ; ++i) {
    DEVMODE candidate_mode;
    if (!GetPrimaryDisplayMode(i, EDS_ROTATEDMODE, &candidate_mode))
      break;

    // Ignore modes missing the fields that we expect.
    if (!IsModeValid(candidate_mode))
      continue;

    // Ignore modes with differing bits-per-pixel.
    if (candidate_mode.dmBitsPerPel != current_mode.dmBitsPerPel)
      continue;

    // If there are multiple modes with the same dimensions:
    // - Prefer the modes which match the current rotation.
    // - Among those, prefer modes which match the current frequency.
    // - Otherwise, prefer modes with a higher frequency.
    ScreenResolution candidate_resolution = GetModeResolution(candidate_mode);
    if (best_mode_for_resolution_.count(candidate_resolution) != 0) {
      DEVMODE best_mode = best_mode_for_resolution_[candidate_resolution];

      if ((candidate_mode.dmDisplayOrientation !=
           current_mode.dmDisplayOrientation) &&
          (best_mode.dmDisplayOrientation ==
           current_mode.dmDisplayOrientation)) {
        continue;
      }

      if ((candidate_mode.dmDisplayFrequency !=
           current_mode.dmDisplayFrequency) &&
          (best_mode.dmDisplayFrequency >=
           candidate_mode.dmDisplayFrequency)) {
        continue;
      }
    } else {
      // If we haven't seen this resolution before, add it to those we return.
      resolutions.push_back(candidate_resolution);
    }

    best_mode_for_resolution_[candidate_resolution] = candidate_mode;
  }

  return resolutions;
}

void DesktopResizerWin::SetResolution(const ScreenResolution& resolution) {
  if (best_mode_for_resolution_.count(resolution) == 0)
    return;

  DEVMODE new_mode = best_mode_for_resolution_[resolution];
  DWORD result = ChangeDisplaySettings(&new_mode, CDS_FULLSCREEN);
  if (result != DISP_CHANGE_SUCCESSFUL)
    LOG(ERROR) << "SetResolution failed: " << result;
}

void DesktopResizerWin::RestoreResolution(const ScreenResolution& original) {
  // Restore the display mode based on the registry configuration.
  DWORD result = ChangeDisplaySettings(nullptr, 0);
  if (result != DISP_CHANGE_SUCCESSFUL)
    LOG(ERROR) << "RestoreResolution failed: " << result;
}

// static
bool DesktopResizerWin::IsResizeSupported() {
  // Resize is supported only on single-monitor systems.
  return GetSystemMetrics(SM_CMONITORS) == 1;
}

// static
bool DesktopResizerWin::GetPrimaryDisplayMode(
    DWORD mode_number, DWORD flags, DEVMODE* mode) {
 memset(mode, 0, sizeof(DEVMODE));
 mode->dmSize = sizeof(DEVMODE);
 if (!EnumDisplaySettingsEx(nullptr, mode_number, mode, flags))
   return false;
 return true;
}

// static
bool DesktopResizerWin::IsModeValid(const DEVMODE& mode) {
  const DWORD kRequiredFields =
      DM_PELSWIDTH | DM_PELSHEIGHT | DM_BITSPERPEL |
      DM_DISPLAYFREQUENCY | DM_DISPLAYORIENTATION;
  return (mode.dmFields & kRequiredFields) == kRequiredFields;
}

// static
ScreenResolution DesktopResizerWin::GetModeResolution(const DEVMODE& mode) {
  DCHECK(IsModeValid(mode));
  return ScreenResolution(
      webrtc::DesktopSize(mode.dmPelsWidth, mode.dmPelsHeight),
      webrtc::DesktopVector(kDefaultDPI, kDefaultDPI));
}

std::unique_ptr<DesktopResizer> DesktopResizer::Create() {
  return base::WrapUnique(new DesktopResizerWin);
}

}  // namespace remoting
