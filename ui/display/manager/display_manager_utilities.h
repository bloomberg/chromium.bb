// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_DISPLAY_MANAGER_DISPLAY_MANAGER_UTILITIES_H_
#define UI_DISPLAY_MANAGER_DISPLAY_MANAGER_UTILITIES_H_

#include <vector>

#include "ui/display/display.h"
#include "ui/display/display_layout.h"
#include "ui/display/manager/display_manager_export.h"
#include "ui/display/manager/managed_display_info.h"

namespace gfx {
class Rect;
class Size;
}

namespace display {
class ManagedDisplayInfo;
class ManagedDisplayMode;
using DisplayInfoList = std::vector<ManagedDisplayInfo>;

// Creates the display mode list for internal display
// based on |native_mode|.
DISPLAY_MANAGER_EXPORT ManagedDisplayInfo::ManagedDisplayModeList
CreateInternalManagedDisplayModeList(const ManagedDisplayMode& native_mode);

// Defines parameters needed to construct a ManagedDisplayMode for Unified
// Desktop.
struct UnifiedDisplayModeParam {
  UnifiedDisplayModeParam(float dsf, float scale, bool is_default);

  float device_scale_factor = 1.0f;

  float display_bounds_scale = 1.0f;

  bool is_default_mode = false;
};

// Creates the display mode list for unified display
// based on |native_mode| and |scales|.
DISPLAY_MANAGER_EXPORT ManagedDisplayInfo::ManagedDisplayModeList
CreateUnifiedManagedDisplayModeList(
    const ManagedDisplayMode& native_mode,
    const std::vector<UnifiedDisplayModeParam>& modes_param_list);

// Returns true if the first display should unconditionally be considered an
// internal display.
bool ForceFirstDisplayInternal();

// Computes the bounds that defines the bounds between two displays.
// Returns false if two displays do not intersect.
DISPLAY_MANAGER_EXPORT bool ComputeBoundary(
    const Display& primary_display,
    const Display& secondary_display,
    gfx::Rect* primary_edge_in_screen,
    gfx::Rect* secondary_edge_in_screen);

// Sorts id list using |CompareDisplayIds| below.
DISPLAY_MANAGER_EXPORT void SortDisplayIdList(DisplayIdList* list);

// Default id generator.
class DefaultDisplayIdGenerator {
 public:
  int64_t operator()(int64_t id) { return id; }
};

// Generate sorted DisplayIdList from iterators.
template <class ForwardIterator, class Generator = DefaultDisplayIdGenerator>
DisplayIdList GenerateDisplayIdList(ForwardIterator first,
                                    ForwardIterator last,
                                    Generator generator = Generator()) {
  DisplayIdList list;
  while (first != last) {
    list.push_back(generator(*first));
    ++first;
  }
  SortDisplayIdList(&list);
  return list;
}

// Creates sorted DisplayIdList.
DISPLAY_MANAGER_EXPORT DisplayIdList CreateDisplayIdList(const Displays& list);

DISPLAY_MANAGER_EXPORT std::string DisplayIdListToString(
    const DisplayIdList& list);

// Creates managed display info.
DISPLAY_MANAGER_EXPORT display::ManagedDisplayInfo CreateDisplayInfo(
    int64_t id,
    const gfx::Rect& bounds);

// Get the display id after the output index (8 bits) is masked out.
DISPLAY_MANAGER_EXPORT int64_t GetDisplayIdWithoutOutputIndex(int64_t id);

// Defines parameters needed to set mixed mirror mode.
struct DISPLAY_MANAGER_EXPORT MixedMirrorModeParams {
  MixedMirrorModeParams(int64_t src_id, const DisplayIdList& dst_ids);
  MixedMirrorModeParams(const MixedMirrorModeParams& mixed_params);
  ~MixedMirrorModeParams();

  int64_t source_id;  // Id of the mirroring source display

  DisplayIdList destination_ids;  // Ids of the mirroring destination displays.
};

// Defines mirror modes used to change the display mode.
enum class MirrorMode {
  kOff = 0,
  kNormal,
  kMixed,
};

// Defines the error types of mixed mirror mode parameters.
enum class MixedMirrorModeParamsErrors {
  kSuccess = 0,
  kErrorSingleDisplay,
  kErrorSourceIdNotFound,
  kErrorDestinationIdsEmpty,
  kErrorDestinationIdNotFound,
  kErrorDuplicateId,
};

// Verifies whether the mixed mirror mode parameters are valid.
// |connected_display_ids| is the id list for all connected displays. Returns
// error type for the parameters.
DISPLAY_MANAGER_EXPORT MixedMirrorModeParamsErrors
ValidateParamsForMixedMirrorMode(
    const DisplayIdList& connected_display_ids,
    const MixedMirrorModeParams& mixed_mode_params);

}  // namespace display

#endif  // UI_DISPLAY_MANAGER_DISPLAY_MANAGER_UTILITIES_H_
