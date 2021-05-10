// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_WINDOW_CYCLE_WINDOW_CYCLE_CONTROLLER_H_
#define ASH_WM_WINDOW_CYCLE_WINDOW_CYCLE_CONTROLLER_H_

#include <memory>

#include "ash/ash_export.h"
#include "ash/public/cpp/session/session_observer.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/wm/mru_window_tracker.h"
#include "base/macros.h"
#include "base/time/time.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_registry_simple.h"

namespace aura {
class Window;
}

namespace ui {
class LocatedEvent;
}

namespace ash {

class WindowCycleEventFilter;
class WindowCycleList;

// Controls cycling through windows with the keyboard via alt-tab.
// Windows are sorted primarily by most recently used, and then by screen order.
// We activate windows as you cycle through them, so the order on the screen
// may change during the gesture, but the most recently used list isn't updated
// until the cycling ends.  Thus we maintain the state of the windows
// at the beginning of the gesture so you can cycle through in a consistent
// order.
class ASH_EXPORT WindowCycleController : public SessionObserver {
 public:
  using WindowList = std::vector<aura::Window*>;

  enum class WindowCyclingDirection { kForward, kBackward };
  enum class KeyboardNavDirection { kUp, kDown, kLeft, kRight, kInvalid };

  // Enumeration of the sources of alt-tab mode switch.
  // Note that these values are persisted to histograms so existing values
  // should remain unchanged and new values should be added to the end.
  enum class ModeSwitchSource { kClick, kKeyboard, kMaxValue = kKeyboard };

  WindowCycleController();
  ~WindowCycleController() override;

  // Returns true if cycling through windows is enabled. This is false at
  // certain times, such as when the lock screen is visible.
  static bool CanCycle();

  // Registers alt-tab related profile prefs.
  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  // Returns the WindowCycleList.
  const WindowCycleList* window_cycle_list() const {
    return window_cycle_list_.get();
  }

  // Cycles between windows in the given |direction|. This moves the focus ring
  // to the window in the given |direction| and also scrolls the list.
  void HandleCycleWindow(WindowCyclingDirection direction);

  // Navigates between cycle windows and tab slider if the move is valid.
  // This moves the focus ring to the active button or the last focused window
  // and announces these changes via ChromeVox.
  void HandleKeyboardNavigation(KeyboardNavDirection direction);

  // Returns true if the direction is valid regarding the component that the
  // focus is currently on. For example, moving the focus on the top most
  // component, the tab slider button, further up is invalid.
  bool IsValidKeyboardNavigation(KeyboardNavDirection direction);

  // Scrolls the windows in the given |direction|. This does not move the focus
  // ring.
  void Scroll(WindowCyclingDirection direction);

  // Drags the cycle view's mirror container |delta_x|.
  void Drag(float delta_x);

  // Returns true if we are in the middle of a window cycling gesture.
  bool IsCycling() const { return window_cycle_list_.get() != NULL; }

  // Call to start cycling windows. This function adds a pre-target handler to
  // listen to the alt key release.
  void StartCycling();

  // Both of these functions stop the current window cycle and removes the event
  // filter. The former indicates success (i.e. the new window should be
  // activated) and the latter indicates that the interaction was cancelled (and
  // the originally active window should remain active).
  void CompleteCycling();
  void CancelCycling();

  // If the window cycle list is open, re-construct it. Do nothing if not
  // cycling.
  void MaybeResetCycleList();

  // Moves the focus ring to |window|. Does not scroll the list. Do nothing if
  // not cycling.
  void SetFocusedWindow(aura::Window* window);

  // Checks whether |event| occurs within the cycle view.
  bool IsEventInCycleView(const ui::LocatedEvent* event);

  // Gets the window for the preview item located at |event|. Returns nullptr if
  // |event| is not on the cycle view or a preview item, or |window_cycle_list_|
  // does not exist.
  aura::Window* GetWindowAtPoint(const ui::LocatedEvent* event);

  // Returns whether or not the window cycle view is visible.
  bool IsWindowListVisible();

  // Checks if switching between alt-tab mode via the tab slider is allowed.
  // Returns true if Bento flag is enabled and users have multiple desks.
  bool IsInteractiveAltTabModeAllowed();

  // Checks if alt-tab should be per active desk. If
  // `IsInteractiveAltTabModeAllowed()`, alt-tab mode depends on users'
  // |prefs::kAltTabPerDesk| selection. Otherwise, it'll default to all desk
  // unless LimitAltTabToActiveDesk flag is explicitly enabled.
  bool IsAltTabPerActiveDesk();

  // Returns true while switching the alt-tab mode and Bento flag is enabled.
  // This helps `Scroll()` and `Step()` distinguish between pressing tabs and
  // switching mode, so they refresh |current_index_| and the highlighted
  // window correctly.
  bool IsSwitchingMode();

  // Returns if the tab slider is currently focused instead of the window cycle
  // during keyboard navigation.
  bool IsTabSliderFocused();

  // SessionObserver:
  void OnActiveUserPrefServiceChanged(PrefService* pref_service) override;

  // Saves |per_desk| in the user prefs and announces changes of alt-tab mode
  // and the window selection via ChromeVox. This function is called when the
  // user switches the alt-tab mode via keyboard navigation or button clicking.
  void OnModeChanged(bool per_desk, ModeSwitchSource source);

 private:
  // Gets a list of windows from the currently open windows, removing windows
  // with transient roots already in the list. The returned list of windows
  // is used to populate the window cycle list.
  WindowList CreateWindowList();

  // Populates |active_desk_container_id_before_cycle_| and
  // |active_window_before_window_cycle_| when the window cycle list is
  // initialized.
  void SaveCurrentActiveDeskAndWindow(const WindowList& window_list);

  // Cycles to the next or previous window based on |direction|.
  void Step(WindowCyclingDirection direction);

  void StopCycling();

  void InitFromUserPrefs();

  // Triggers alt-tab UI updates when the alt-tab mode is updated in the active
  // user prefs.
  void OnAltTabModePrefChanged();

  std::unique_ptr<WindowCycleList> window_cycle_list_;

  // Tracks the ID of the active desk container before window cycling starts. It
  // is used to determine whether a desk switch occurred when cycling ends.
  int active_desk_container_id_before_cycle_ = kShellWindowId_Invalid;

  // Tracks what Window was active when starting to cycle and used to determine
  // if the active Window changed in when ending cycling.
  aura::Window* active_window_before_window_cycle_ = nullptr;

  // Non-null while actively cycling.
  std::unique_ptr<WindowCycleEventFilter> event_filter_;

  // Tracks whether alt-tab mode is currently switching or not.
  bool is_switching_mode_ = false;

  // The pref service of the currently active user. Can be null in
  // ash_unittests.
  PrefService* active_user_pref_service_ = nullptr;

  // The pref change registrar to observe changes in prefs value.
  std::unique_ptr<PrefChangeRegistrar> pref_change_registrar_;

  DISALLOW_COPY_AND_ASSIGN(WindowCycleController);
};

}  // namespace ash

#endif  // ASH_WM_WINDOW_CYCLE_WINDOW_CYCLE_CONTROLLER_H_
