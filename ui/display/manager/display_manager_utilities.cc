// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/manager/display_manager_utilities.h"

#include <algorithm>
#include <vector>

#include "base/sys_info.h"
#include "ui/display/manager/managed_display_info.h"
#include "ui/gfx/geometry/size_conversions.h"
#include "ui/gfx/geometry/size_f.h"

namespace display {

namespace {

// List of value UI Scale values. Scales for 2x are equivalent to 640,
// 800, 1024, 1280, 1440, 1600 and 1920 pixel width respectively on
// 2560 pixel width 2x density display. Please see crbug.com/233375
// for the full list of resolutions.
constexpr float kUIScalesFor2x[] = {0.5f,   0.625f, 0.8f, 1.0f,
                                    1.125f, 1.25f,  1.5f, 2.0f};
constexpr float kUIScalesFor1_25x[] = {0.5f, 0.625f, 0.8f, 1.0f, 1.25f};
constexpr float kUIScalesFor1_6x[] = {0.5f, 0.8f, 1.0f, 1.2f, 1.6f};

constexpr float kUIScalesFor1280[] = {0.5f, 0.625f, 0.8f, 1.0f, 1.125f};
constexpr float kUIScalesFor1366[] = {0.5f, 0.6f, 0.75f, 1.0f, 1.125f};
constexpr float kUIScalesForFHD[] = {0.5f, 0.625f, 0.8f, 1.0f, 1.25f};

// The default UI scales for the above display densities.
constexpr float kDefaultUIScaleFor2x = 1.0f;
constexpr float kDefaultUIScaleFor1_25x = 0.8f;
constexpr float kDefaultUIScaleFor1_6x = 1.0f;
constexpr float kDefaultUIScaleFor1280 = 1.0f;
constexpr float kDefaultUIScaleFor1366 = 1.0f;
constexpr float kDefaultUIScaleForFHD = 1.0f;

// Encapsulates the list of UI scales and the default one.
struct DisplayUIScales {
  std::vector<float> scales;
  float default_scale;
};

DisplayUIScales GetScalesForDisplay(
    const scoped_refptr<ManagedDisplayMode>& native_mode) {
#define ASSIGN_ARRAY(v, a) v.assign(a, a + arraysize(a))

  DisplayUIScales ret;
  if (native_mode->device_scale_factor() == 2.0f) {
    ASSIGN_ARRAY(ret.scales, kUIScalesFor2x);
    ret.default_scale = kDefaultUIScaleFor2x;
    return ret;
  } else if (native_mode->device_scale_factor() == 1.25f) {
    ASSIGN_ARRAY(ret.scales, kUIScalesFor1_25x);
    ret.default_scale = kDefaultUIScaleFor1_25x;
    return ret;
  } else if (native_mode->device_scale_factor() == 1.6f) {
    ASSIGN_ARRAY(ret.scales, kUIScalesFor1_6x);
    ret.default_scale = kDefaultUIScaleFor1_6x;
    return ret;
  }
  switch (native_mode->size().width()) {
    case 1280:
      ASSIGN_ARRAY(ret.scales, kUIScalesFor1280);
      ret.default_scale = kDefaultUIScaleFor1280;
      break;
    case 1366:
      ASSIGN_ARRAY(ret.scales, kUIScalesFor1366);
      ret.default_scale = kDefaultUIScaleFor1366;
      break;
    case 1980:
      ASSIGN_ARRAY(ret.scales, kUIScalesForFHD);
      ret.default_scale = kDefaultUIScaleForFHD;
      break;
    default:
      ASSIGN_ARRAY(ret.scales, kUIScalesFor1280);
      ret.default_scale = kDefaultUIScaleFor1280;
#if defined(OS_CHROMEOS)
      if (base::SysInfo::IsRunningOnChromeOS())
        NOTREACHED() << "Unknown resolution:" << native_mode->size().ToString();
#endif
  }
  return ret;
}

struct ScaleComparator {
  explicit ScaleComparator(float s) : scale(s) {}

  bool operator()(const scoped_refptr<ManagedDisplayMode>& mode) const {
    const float kEpsilon = 0.0001f;
    return std::abs(scale - mode->ui_scale()) < kEpsilon;
  }
  float scale;
};

scoped_refptr<ManagedDisplayMode> FindNextMode(
    const ManagedDisplayInfo::ManagedDisplayModeList& modes,
    size_t index,
    bool up) {
  DCHECK_LT(index, modes.size());
  size_t new_index = index;
  if (up && (index + 1 < modes.size()))
    ++new_index;
  else if (!up && index != 0)
    --new_index;
  return modes[new_index];
}

}  // namespace

ManagedDisplayInfo::ManagedDisplayModeList CreateInternalManagedDisplayModeList(
    const scoped_refptr<ManagedDisplayMode>& native_mode) {
  ManagedDisplayInfo::ManagedDisplayModeList display_mode_list;

  float native_ui_scale = (native_mode->device_scale_factor() == 1.25f)
                              ? 1.0f
                              : native_mode->device_scale_factor();
  const DisplayUIScales display_ui_scales = GetScalesForDisplay(native_mode);
  for (float ui_scale : display_ui_scales.scales) {
    scoped_refptr<ManagedDisplayMode> mode(new ManagedDisplayMode(
        native_mode->size(), native_mode->refresh_rate(),
        native_mode->is_interlaced(), ui_scale == native_ui_scale, ui_scale,
        native_mode->device_scale_factor()));
    mode->set_is_default(ui_scale == display_ui_scales.default_scale);
    display_mode_list.push_back(mode);
  }
  return display_mode_list;
}

ManagedDisplayInfo::ManagedDisplayModeList CreateUnifiedManagedDisplayModeList(
    const scoped_refptr<ManagedDisplayMode>& native_mode,
    const std::set<std::pair<float, float>>& dsf_scale_list) {
  ManagedDisplayInfo::ManagedDisplayModeList display_mode_list;

  for (auto& pair : dsf_scale_list) {
    gfx::SizeF scaled_size(native_mode->size());
    scaled_size.Scale(pair.second);
    scoped_refptr<ManagedDisplayMode> mode(new ManagedDisplayMode(
        gfx::ToFlooredSize(scaled_size), native_mode->refresh_rate(),
        native_mode->is_interlaced(), false /* native */,
        native_mode->ui_scale(), pair.first /* device_scale_factor */));
    display_mode_list.push_back(mode);
  }
  // Sort the mode by the size in DIP.
  std::sort(display_mode_list.begin(), display_mode_list.end(),
            [](const scoped_refptr<ManagedDisplayMode>& a,
               const scoped_refptr<ManagedDisplayMode>& b) {
              return a->GetSizeInDIP(false).GetArea() <
                     b->GetSizeInDIP(false).GetArea();
            });
  return display_mode_list;
}

scoped_refptr<ManagedDisplayMode> GetDisplayModeForResolution(
    const ManagedDisplayInfo& info,
    const gfx::Size& resolution) {
  if (Display::IsInternalDisplayId(info.id()))
    return scoped_refptr<ManagedDisplayMode>();

  const ManagedDisplayInfo::ManagedDisplayModeList& modes =
      info.display_modes();
  DCHECK_NE(0u, modes.size());
  scoped_refptr<ManagedDisplayMode> target_mode;
  ManagedDisplayInfo::ManagedDisplayModeList::const_iterator iter =
      std::find_if(modes.begin(), modes.end(),
                   [resolution](const scoped_refptr<ManagedDisplayMode>& mode) {
                     return mode->size() == resolution;
                   });
  if (iter == modes.end()) {
    LOG(WARNING) << "Unsupported resolution was requested:"
                 << resolution.ToString();
    return scoped_refptr<ManagedDisplayMode>();
  }
  return *iter;
}

scoped_refptr<ManagedDisplayMode> GetDisplayModeForNextUIScale(
    const ManagedDisplayInfo& info,
    bool up) {
  const ManagedDisplayInfo::ManagedDisplayModeList& modes =
      info.display_modes();
  ScaleComparator comparator(info.configured_ui_scale());
  auto iter = std::find_if(modes.begin(), modes.end(), comparator);
  return FindNextMode(modes, iter - modes.begin(), up);
}

scoped_refptr<ManagedDisplayMode> GetDisplayModeForNextResolution(
    const ManagedDisplayInfo& info,
    bool up) {
  if (Display::IsInternalDisplayId(info.id()))
    return scoped_refptr<ManagedDisplayMode>();

  const ManagedDisplayInfo::ManagedDisplayModeList& modes =
      info.display_modes();
  scoped_refptr<ManagedDisplayMode> tmp = new ManagedDisplayMode(
      info.size_in_pixel(), 0.0, false, false, 1.0, info.device_scale_factor());
  gfx::Size resolution = tmp->GetSizeInDIP(false);

  auto iter =
      std::find_if(modes.begin(), modes.end(),
                   [resolution](const scoped_refptr<ManagedDisplayMode>& mode) {
                     return mode->GetSizeInDIP(false) == resolution;
                   });
  return FindNextMode(modes, iter - modes.begin(), up);
}

bool HasDisplayModeForUIScale(const ManagedDisplayInfo& info, float ui_scale) {
  ScaleComparator comparator(ui_scale);
  const ManagedDisplayInfo::ManagedDisplayModeList& modes =
      info.display_modes();
  return std::find_if(modes.begin(), modes.end(), comparator) != modes.end();
}

bool ComputeBoundary(const Display& a_display,
                     const Display& b_display,
                     gfx::Rect* a_edge_in_screen,
                     gfx::Rect* b_edge_in_screen) {
  const gfx::Rect& a_bounds = a_display.bounds();
  const gfx::Rect& b_bounds = b_display.bounds();

  // Find touching side.
  int rx = std::max(a_bounds.x(), b_bounds.x());
  int ry = std::max(a_bounds.y(), b_bounds.y());
  int rr = std::min(a_bounds.right(), b_bounds.right());
  int rb = std::min(a_bounds.bottom(), b_bounds.bottom());

  DisplayPlacement::Position position;
  if ((rb - ry) == 0) {
    // top bottom
    if (a_bounds.bottom() == b_bounds.y()) {
      position = DisplayPlacement::BOTTOM;
    } else if (a_bounds.y() == b_bounds.bottom()) {
      position = DisplayPlacement::TOP;
    } else {
      return false;
    }
  } else {
    // left right
    if (a_bounds.right() == b_bounds.x()) {
      position = DisplayPlacement::RIGHT;
    } else if (a_bounds.x() == b_bounds.right()) {
      position = DisplayPlacement::LEFT;
    } else {
      DCHECK_NE(rr, rx);
      return false;
    }
  }

  switch (position) {
    case DisplayPlacement::TOP:
    case DisplayPlacement::BOTTOM: {
      int left = std::max(a_bounds.x(), b_bounds.x());
      int right = std::min(a_bounds.right(), b_bounds.right());
      if (position == DisplayPlacement::TOP) {
        a_edge_in_screen->SetRect(left, a_bounds.y(), right - left, 1);
        b_edge_in_screen->SetRect(left, b_bounds.bottom() - 1, right - left, 1);
      } else {
        a_edge_in_screen->SetRect(left, a_bounds.bottom() - 1, right - left, 1);
        b_edge_in_screen->SetRect(left, b_bounds.y(), right - left, 1);
      }
      break;
    }
    case DisplayPlacement::LEFT:
    case DisplayPlacement::RIGHT: {
      int top = std::max(a_bounds.y(), b_bounds.y());
      int bottom = std::min(a_bounds.bottom(), b_bounds.bottom());
      if (position == DisplayPlacement::LEFT) {
        a_edge_in_screen->SetRect(a_bounds.x(), top, 1, bottom - top);
        b_edge_in_screen->SetRect(b_bounds.right() - 1, top, 1, bottom - top);
      } else {
        a_edge_in_screen->SetRect(a_bounds.right() - 1, top, 1, bottom - top);
        b_edge_in_screen->SetRect(b_bounds.x(), top, 1, bottom - top);
      }
      break;
    }
  }
  return true;
}

DisplayIdList CreateDisplayIdList(const Displays& list) {
  return GenerateDisplayIdList(
      list.begin(), list.end(),
      [](const Display& display) { return display.id(); });
}

void SortDisplayIdList(DisplayIdList* ids) {
  std::sort(ids->begin(), ids->end(),
            [](int64_t a, int64_t b) { return CompareDisplayIds(a, b); });
}

std::string DisplayIdListToString(const DisplayIdList& list) {
  std::stringstream s;
  const char* sep = "";
  for (int64_t id : list) {
    s << sep << id;
    sep = ",";
  }
  return s.str();
}

}  // namespace display
