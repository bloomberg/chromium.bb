// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/desktop_resizer.h"

#include <map>

#include "base/logging.h"

// Provide comparison operation for SkISize so we can use it in std::map.
static inline bool operator <(const SkISize& a, const SkISize& b) {
  if (a.width() != b.width())
    return a.width() < b.width();
  return a.height() < b.height();
}

namespace remoting {

class DesktopResizerWin : public DesktopResizer {
 public:
  DesktopResizerWin();
  virtual ~DesktopResizerWin();

  // DesktopResizer interface.
  virtual SkISize GetCurrentSize() OVERRIDE;
  virtual std::list<SkISize> GetSupportedSizes(
      const SkISize& preferred) OVERRIDE;
  virtual void SetSize(const SkISize& size) OVERRIDE;
  virtual void RestoreSize(const SkISize& original) OVERRIDE;

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
  static SkISize GetModeSize(const DEVMODE& mode);

  std::map<SkISize, DEVMODE> best_mode_for_size_;

  DISALLOW_COPY_AND_ASSIGN(DesktopResizerWin);
};

DesktopResizerWin::DesktopResizerWin() {
}

DesktopResizerWin::~DesktopResizerWin() {
}

SkISize DesktopResizerWin::GetCurrentSize() {
  DEVMODE current_mode;
  if (GetPrimaryDisplayMode(ENUM_CURRENT_SETTINGS, 0, &current_mode) &&
      IsModeValid(current_mode))
    return GetModeSize(current_mode);
  return SkISize::Make(0, 0);
}

std::list<SkISize> DesktopResizerWin::GetSupportedSizes(
    const SkISize& preferred) {
  if (!IsResizeSupported())
    return std::list<SkISize>();

  // Enumerate the sizes to return, and where there are multiple modes of
  // the same size, store the one most closely matching the current mode
  // in |best_mode_for_size_|.
  DEVMODE current_mode;
  if (!GetPrimaryDisplayMode(ENUM_CURRENT_SETTINGS, 0, &current_mode) ||
      !IsModeValid(current_mode))
    return std::list<SkISize>();

  std::list<SkISize> sizes;
  best_mode_for_size_.clear();
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
    SkISize candidate_size = GetModeSize(candidate_mode);
    if (best_mode_for_size_.count(candidate_size) != 0) {
      DEVMODE best_mode = best_mode_for_size_[candidate_size];

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
      // If we haven't seen this size before, add it to those we return.
      sizes.push_back(candidate_size);
    }

    best_mode_for_size_[candidate_size] = candidate_mode;
  }

  return sizes;
}

void DesktopResizerWin::SetSize(const SkISize& size) {
  if (best_mode_for_size_.count(size) == 0)
    return;

  DEVMODE new_mode = best_mode_for_size_[size];
  DWORD result = ChangeDisplaySettings(&new_mode, CDS_FULLSCREEN);
  if (result != DISP_CHANGE_SUCCESSFUL)
    LOG(ERROR) << "SetSize failed: " << result;
}

void DesktopResizerWin::RestoreSize(const SkISize& original) {
  // Restore the display mode based on the registry configuration.
  DWORD result = ChangeDisplaySettings(NULL, 0);
  if (result != DISP_CHANGE_SUCCESSFUL)
    LOG(ERROR) << "RestoreSize failed: " << result;
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
 if (!EnumDisplaySettingsEx(NULL, mode_number, mode, flags))
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
SkISize DesktopResizerWin::GetModeSize(const DEVMODE& mode) {
  DCHECK(IsModeValid(mode));
  return SkISize::Make(mode.dmPelsWidth, mode.dmPelsHeight);
}

scoped_ptr<DesktopResizer> DesktopResizer::Create() {
  return scoped_ptr<DesktopResizer>(new DesktopResizerWin);
}

}  // namespace remoting
