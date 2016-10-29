// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/test/display_manager_test_api.h"

#include <cstdarg>
#include <vector>

#include "base/strings/string_split.h"
#include "ui/display/display.h"
#include "ui/display/manager/display_layout_builder.h"
#include "ui/display/manager/display_manager.h"
#include "ui/display/manager/display_manager_utilities.h"
#include "ui/display/manager/managed_display_info.h"
#include "ui/display/screen.h"

namespace display {
namespace test {
namespace {

DisplayInfoList CreateDisplayInfoListFromString(
    const std::string specs,
    DisplayManager* display_manager) {
  DisplayInfoList display_info_list;
  std::vector<std::string> parts = base::SplitString(
      specs, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  size_t index = 0;

  display::Displays list =
      display_manager->IsInUnifiedMode()
          ? display_manager->software_mirroring_display_list()
          : display_manager->active_display_list();

  for (std::vector<std::string>::const_iterator iter = parts.begin();
       iter != parts.end(); ++iter, ++index) {
    int64_t id = (index < list.size()) ? list[index].id()
                                       : display::Display::kInvalidDisplayID;
    display_info_list.push_back(
        display::ManagedDisplayInfo::CreateFromSpecWithID(*iter, id));
  }
  return display_info_list;
}

scoped_refptr<display::ManagedDisplayMode> GetDisplayModeForUIScale(
    const display::ManagedDisplayInfo& info,
    float ui_scale) {
  const display::ManagedDisplayInfo::ManagedDisplayModeList& modes =
      info.display_modes();
  auto iter = std::find_if(
      modes.begin(), modes.end(),
      [ui_scale](const scoped_refptr<display::ManagedDisplayMode>& mode) {
        return mode->ui_scale() == ui_scale;
      });
  if (iter == modes.end())
    return scoped_refptr<display::ManagedDisplayMode>();
  return *iter;
}

}  // namespace

DisplayManagerTestApi::DisplayManagerTestApi(DisplayManager* display_manager)
    : display_manager_(display_manager) {}

DisplayManagerTestApi::~DisplayManagerTestApi() {}

void DisplayManagerTestApi::UpdateDisplay(const std::string& display_specs) {
  DisplayInfoList display_info_list =
      CreateDisplayInfoListFromString(display_specs, display_manager_);
  bool is_host_origin_set = false;
  for (size_t i = 0; i < display_info_list.size(); ++i) {
    const display::ManagedDisplayInfo& display_info = display_info_list[i];
    if (display_info.bounds_in_native().origin() != gfx::Point(0, 0)) {
      is_host_origin_set = true;
      break;
    }
  }

  // On non-testing environment, when a secondary display is connected, a new
  // native (i.e. X) window for the display is always created below the
  // previous one for GPU performance reasons. Try to emulate the behavior
  // unless host origins are explicitly set.
  if (!is_host_origin_set) {
    // Start from (1,1) so that windows won't overlap with native mouse cursor.
    // See |AshTestBase::SetUp()|.
    int next_y = 1;
    for (DisplayInfoList::iterator iter = display_info_list.begin();
         iter != display_info_list.end(); ++iter) {
      gfx::Rect bounds(iter->bounds_in_native().size());
      bounds.set_x(1);
      bounds.set_y(next_y);
      next_y += bounds.height();
      iter->SetBounds(bounds);
    }
  }

  display_manager_->OnNativeDisplaysChanged(display_info_list);
  display_manager_->UpdateInternalManagedDisplayModeListForTest();
  display_manager_->RunPendingTasksForTest();
}

int64_t DisplayManagerTestApi::SetFirstDisplayAsInternalDisplay() {
  const display::Display& internal = display_manager_->active_display_list_[0];
  SetInternalDisplayId(internal.id());
  return display::Display::InternalDisplayId();
}

void DisplayManagerTestApi::SetInternalDisplayId(int64_t id) {
  display::Display::SetInternalDisplayId(id);
  display_manager_->UpdateInternalManagedDisplayModeListForTest();
}

void DisplayManagerTestApi::DisableChangeDisplayUponHostResize() {
  display_manager_->set_change_display_upon_host_resize(false);
}

void DisplayManagerTestApi::SetAvailableColorProfiles(
    int64_t display_id,
    const std::vector<ui::ColorCalibrationProfile>& profiles) {
  display_manager_->display_info_[display_id].set_available_color_profiles(
      profiles);
}

const display::ManagedDisplayInfo&
DisplayManagerTestApi::GetInternalManagedDisplayInfo(int64_t display_id) {
  return display_manager_->display_info_[display_id];
}

bool DisplayManagerTestApi::SetDisplayUIScale(int64_t id, float ui_scale) {
  if (!display_manager_->IsActiveDisplayId(id) ||
      !display::Display::IsInternalDisplayId(id)) {
    return false;
  }
  const display::ManagedDisplayInfo& info =
      display_manager_->GetDisplayInfo(id);

  scoped_refptr<display::ManagedDisplayMode> mode =
      GetDisplayModeForUIScale(info, ui_scale);
  if (!mode)
    return false;
  return display_manager_->SetDisplayMode(id, mode);
}

ScopedDisable125DSFForUIScaling::ScopedDisable125DSFForUIScaling() {
  display::ManagedDisplayInfo::SetUse125DSFForUIScalingForTest(false);
}

ScopedDisable125DSFForUIScaling::~ScopedDisable125DSFForUIScaling() {
  display::ManagedDisplayInfo::SetUse125DSFForUIScalingForTest(true);
}

ScopedSetInternalDisplayId::ScopedSetInternalDisplayId(
    DisplayManager* display_manager,
    int64_t id) {
  DisplayManagerTestApi(display_manager).SetInternalDisplayId(id);
}

ScopedSetInternalDisplayId::~ScopedSetInternalDisplayId() {
  display::Display::SetInternalDisplayId(display::Display::kInvalidDisplayID);
}

bool SetDisplayResolution(DisplayManager* display_manager,
                          int64_t display_id,
                          const gfx::Size& resolution) {
  const display::ManagedDisplayInfo& info =
      display_manager->GetDisplayInfo(display_id);
  scoped_refptr<display::ManagedDisplayMode> mode =
      GetDisplayModeForResolution(info, resolution);
  if (!mode)
    return false;
  return display_manager->SetDisplayMode(display_id, mode);
}

std::unique_ptr<display::DisplayLayout> CreateDisplayLayout(
    DisplayManager* display_manager,
    display::DisplayPlacement::Position position,
    int offset) {
  display::DisplayLayoutBuilder builder(
      display::Screen::GetScreen()->GetPrimaryDisplay().id());
  builder.SetSecondaryPlacement(display_manager->GetSecondaryDisplay().id(),
                                position, offset);
  return builder.Build();
}

display::DisplayIdList CreateDisplayIdList2(int64_t id1, int64_t id2) {
  display::DisplayIdList list;
  list.push_back(id1);
  list.push_back(id2);
  display::SortDisplayIdList(&list);
  return list;
}

display::DisplayIdList CreateDisplayIdListN(size_t count, ...) {
  display::DisplayIdList list;
  va_list args;
  va_start(args, count);
  for (size_t i = 0; i < count; i++) {
    int64_t id = va_arg(args, int64_t);
    list.push_back(id);
  }
  display::SortDisplayIdList(&list);
  return list;
}

}  // namespace test
}  // namespace display
