// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_DISPLAY_SCREEN_INFOS_H_
#define UI_DISPLAY_SCREEN_INFOS_H_

#include "ui/display/display_export.h"
#include "ui/display/screen_info.h"

namespace display {

// Information about a set of screens that are relevant to a particular widget.
// This includes an id for the screen currently showing the widget.
// This structure roughly parallels display::DisplayList. It may be desirable to
// deprecate derived counterparts of ui/display types; see crbug.com/1208469.
struct DISPLAY_EXPORT ScreenInfos {
  ScreenInfos();
  explicit ScreenInfos(const ScreenInfo& screen_info);
  ScreenInfos(const ScreenInfos& other);
  ~ScreenInfos();
  ScreenInfos& operator=(const ScreenInfos& other);

  bool operator==(const ScreenInfos& other) const;
  bool operator!=(const ScreenInfos& other) const;

  // Helpers to access the current ScreenInfo element.
  ScreenInfo& mutable_current();
  const ScreenInfo& current() const;

  std::vector<ScreenInfo> screen_infos;
  // The display_id of the current ScreenInfo in `screen_infos`.
  int64_t current_display_id = kInvalidDisplayId;
};

}  // namespace display

#endif  // UI_DISPLAY_SCREEN_INFOS_H_
