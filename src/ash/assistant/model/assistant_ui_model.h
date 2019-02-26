// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ASSISTANT_MODEL_ASSISTANT_UI_MODEL_H_
#define ASH_ASSISTANT_MODEL_ASSISTANT_UI_MODEL_H_

#include "base/macros.h"
#include "base/observer_list.h"
#include "ui/gfx/geometry/rect.h"

namespace ash {

class AssistantUiModelObserver;

// Enumeration of Assistant entry points. These values are persisted to logs.
// Entries should not be renumbered and  numeric values should never be reused.
// Only append to this enum is allowed if the possible entry source grows.
enum class AssistantEntryPoint {
  kUnspecified = 0,
  kDeepLink = 1,
  kHotkey = 2,
  kHotword = 3,
  kLauncherSearchBox = 4,
  kLongPressLauncher = 5,
  kSetup = 6,
  kStylus = 7,
  // Special enumerator value used by histogram macros.
  kMaxValue = kStylus
};

// Enumeration of Assistant exit points. These values are persisted to logs.
// Entries should not be renumbered and numeric values should never be reused.
// Only append to this enum is allowed if the possible exit source grows.
enum class AssistantExitPoint {
  // Includes keyboard interruptions (e.g. launching Chrome OS feedback
  // using keyboard shortcuts, pressing search button).
  kUnspecified = 0,
  kCloseButton = 1,
  kHotkey = 2,
  kNewBrowserTabFromServer = 3,
  kNewBrowserTabFromUser = 4,
  kOutsidePress = 5,
  kSetup = 6,
  kStylus = 7,
  // Special enumerator value used by histogram macros.
  kMaxValue = kStylus
};

// Enumeration of Assistant UI modes.
enum class AssistantUiMode {
  kMainUi,
  kMiniUi,
  kWebUi,
};

// Enumeration of Assistant visibility states.
enum class AssistantVisibility {
  kClosed,   // Assistant UI is hidden and the previous session has finished.
  kHidden,   // Assistant UI is hidden and the previous session is paused.
  kVisible,  // Assistant UI is visible and a session is in progress.
};

// Models the Assistant UI.
class AssistantUiModel {
 public:
  AssistantUiModel();
  ~AssistantUiModel();

  // Adds/removes the specified |observer|.
  void AddObserver(AssistantUiModelObserver* observer);
  void RemoveObserver(AssistantUiModelObserver* observer);

  // Sets the UI mode.
  void SetUiMode(AssistantUiMode ui_mode);

  // Returns the UI mode.
  AssistantUiMode ui_mode() const { return ui_mode_; }

  // Sets the UI visibility.
  void SetVisible(AssistantEntryPoint entry_point);
  void SetHidden(AssistantExitPoint exit_point);
  void SetClosed(AssistantExitPoint exit_point);

  AssistantVisibility visibility() const { return visibility_; }

  // Sets the current usable work area.
  void SetUsableWorkArea(const gfx::Rect& usable_work_area);

  // Returns the current usable work area.
  const gfx::Rect& usable_work_area() const { return usable_work_area_; }

  // Returns the UI entry point. Only valid while UI is visible.
  AssistantEntryPoint entry_point() const { return entry_point_; }

 private:
  void SetVisibility(AssistantVisibility visibility,
                     base::Optional<AssistantEntryPoint> entry_point,
                     base::Optional<AssistantExitPoint> exit_point);

  void NotifyUiModeChanged();
  void NotifyUiVisibilityChanged(
      AssistantVisibility old_visibility,
      base::Optional<AssistantEntryPoint> entry_point,
      base::Optional<AssistantExitPoint> exit_point);
  void NotifyUsableWorkAreaChanged();

  AssistantUiMode ui_mode_ = AssistantUiMode::kMainUi;

  AssistantVisibility visibility_ = AssistantVisibility::kClosed;

  AssistantEntryPoint entry_point_ = AssistantEntryPoint::kUnspecified;

  base::ObserverList<AssistantUiModelObserver> observers_;

  // Usable work area for Assistant. Value is only meaningful when Assistant
  // UI exists.
  gfx::Rect usable_work_area_;

  DISALLOW_COPY_AND_ASSIGN(AssistantUiModel);
};

}  // namespace ash

#endif  // ASH_ASSISTANT_MODEL_ASSISTANT_UI_MODEL_H_
