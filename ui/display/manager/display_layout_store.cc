// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdio.h>

#include <string>
#include <utility>

#include "base/command_line.h"
#include "base/logging.h"
#include "ui/display/display.h"
#include "ui/display/display_switches.h"
#include "ui/display/manager/display_layout_store.h"

namespace display {

namespace {

// TODO(kylechar): Move these to ui/display/chromeos/display_util.cc/h.
bool CompareDisplayIds(int64_t id1, int64_t id2) {
  DCHECK_NE(id1, id2);
  // Output index is stored in the first 8 bits. See GetDisplayIdFromEDID
  // in edid_parser.cc.
  int index_1 = id1 & 0xFF;
  int index_2 = id2 & 0xFF;
  DCHECK_NE(index_1, index_2) << id1 << " and " << id2;
  return Display::IsInternalDisplayId(id1) ||
         (index_1 < index_2 && !Display::IsInternalDisplayId(id2));
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

}  // namespace

DisplayLayoutStore::DisplayLayoutStore()
    : default_display_placement_(display::DisplayPlacement::RIGHT, 0) {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kSecondaryDisplayLayout)) {
    std::string value =
        command_line->GetSwitchValueASCII(switches::kSecondaryDisplayLayout);
    char layout;
    int offset = 0;
    if (sscanf(value.c_str(), "%c,%d", &layout, &offset) == 2) {
      if (layout == 't')
        default_display_placement_.position = display::DisplayPlacement::TOP;
      else if (layout == 'b')
        default_display_placement_.position = display::DisplayPlacement::BOTTOM;
      else if (layout == 'r')
        default_display_placement_.position = display::DisplayPlacement::RIGHT;
      else if (layout == 'l')
        default_display_placement_.position = display::DisplayPlacement::LEFT;
      default_display_placement_.offset = offset;
    }
  }
}

DisplayLayoutStore::~DisplayLayoutStore() {}

void DisplayLayoutStore::SetDefaultDisplayPlacement(
    const display::DisplayPlacement& placement) {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (!command_line->HasSwitch(switches::kSecondaryDisplayLayout))
    default_display_placement_ = placement;
}

void DisplayLayoutStore::RegisterLayoutForDisplayIdList(
    const display::DisplayIdList& list,
    std::unique_ptr<display::DisplayLayout> layout) {
  // m50/51 dev/beta channel may have bad layout data saved in local state.
  // TODO(oshima): Consider removing this after m53.
  if (list.size() == 2 && layout->placement_list.size() > 1)
    return;

  // Do not overwrite the valid data with old invalid date.
  if (layouts_.count(list) && !CompareDisplayIds(list[0], list[1]))
    return;

  // Old data may not have the display_id/parent_display_id.
  // Guess these values based on the saved primary_id.
  if (layout->placement_list.size() >= 1 &&
      layout->placement_list[0].display_id ==
          display::Display::kInvalidDisplayID) {
    if (layout->primary_id == list[1]) {
      layout->placement_list[0].display_id = list[0];
      layout->placement_list[0].parent_display_id = list[1];
    } else {
      layout->placement_list[0].display_id = list[1];
      layout->placement_list[0].parent_display_id = list[0];
    }
  }
  DCHECK(display::DisplayLayout::Validate(list, *layout.get()))
      << "ids=" << DisplayIdListToString(list)
      << ", layout=" << layout->ToString();
  layouts_[list] = std::move(layout);
}

const display::DisplayLayout& DisplayLayoutStore::GetRegisteredDisplayLayout(
    const display::DisplayIdList& list) {
  DCHECK_NE(1u, list.size());
  const auto iter = layouts_.find(list);
  const display::DisplayLayout* layout = iter != layouts_.end()
                                             ? iter->second.get()
                                             : CreateDefaultDisplayLayout(list);
  DCHECK(display::DisplayLayout::Validate(list, *layout)) << layout->ToString();
  DCHECK_NE(layout->primary_id, display::Display::kInvalidDisplayID);
  return *layout;
}

void DisplayLayoutStore::UpdateMultiDisplayState(
    const display::DisplayIdList& list,
    bool mirrored,
    bool default_unified) {
  DCHECK(layouts_.find(list) != layouts_.end());
  if (layouts_.find(list) == layouts_.end())
    CreateDefaultDisplayLayout(list);

  layouts_[list]->mirrored = mirrored;
  layouts_[list]->default_unified = default_unified;
}

display::DisplayLayout* DisplayLayoutStore::CreateDefaultDisplayLayout(
    const display::DisplayIdList& list) {
  std::unique_ptr<display::DisplayLayout> layout(new display::DisplayLayout);
  // The first display is the primary by default.
  layout->primary_id = list[0];
  layout->placement_list.clear();
  for (size_t i = 0; i < list.size() - 1; i++) {
    display::DisplayPlacement placement(default_display_placement_);
    placement.display_id = list[i + 1];
    placement.parent_display_id = list[i];
    layout->placement_list.push_back(placement);
  }
  layouts_[list] = std::move(layout);
  auto iter = layouts_.find(list);
  return iter->second.get();
}

}  // namespace display
