// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_EXCLUSIVE_ACCESS_KEYBOARD_LOCK_CONTROLLER_H_
#define CHROME_BROWSER_UI_EXCLUSIVE_ACCESS_KEYBOARD_LOCK_CONTROLLER_H_

#include "base/callback.h"
#include "base/containers/circular_deque.h"
#include "base/macros.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_bubble_hide_callback.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_controller_base.h"

namespace base {
class TickClock;
}  // namespace base

namespace content {
struct NativeWebKeyboardEvent;
class WebContents;
}  // namespace content

// This class implements keyboard lock behavior in the UI and decides whether
// a KeyboardLock request from a WebContents instance should be allowed or
// rejected.
class KeyboardLockController : public ExclusiveAccessControllerBase {
 public:
  explicit KeyboardLockController(ExclusiveAccessManager* manager);
  ~KeyboardLockController() override;

  // Requests KeyboardLock for |web_contents|, request is allowed if
  // |web_contents| is in tab-initiated fullscreen.
  void RequestKeyboardLock(content::WebContents* web_contents,
                           bool esc_key_locked);

  // Cancels an existing request for keyboard lock for |web_contents|.
  void CancelKeyboardLockRequest(content::WebContents* web_contents);

  // ExclusiveAccessControllerBase implementation.
  bool HandleUserPressedEscape() override;
  void ExitExclusiveAccessToPreviousState() override;
  void ExitExclusiveAccessIfNecessary() override;
  void NotifyTabExclusiveAccessLost() override;
  void RecordBubbleReshowsHistogram(int bubble_reshow_count) override;

  // Returns true if the keyboard is locked.
  bool IsKeyboardLockActive() const;

  // Returns true if the user must press and hold esc to exit keyboard lock.
  bool RequiresPressAndHoldEscToExit() const;

  // Notifies KeyboardLockController instance that the current tab has lost
  // exclusive access.
  void LostKeyboardLock();

  // Allows for special handling for KeyDown/KeyUp events.  Returns true if the
  // event was handled by the KeyboardLockController.
  bool HandleKeyEvent(const content::NativeWebKeyboardEvent& event);

 private:
  friend class FullscreenControllerTest;
  friend class FullscreenControlViewTest;

  enum class KeyboardLockState {
    kUnlocked,
    kLockedWithEsc,
    kLockedWithoutEsc,
  };

  // Notifies |web_contents| that it can activate keyboard lock.
  void LockKeyboard(content::WebContents* web_contents, bool esc_key_locked);

  // Notifies the exclusive access tab that it must deactivate keyboard lock.
  void UnlockKeyboard();

  // Called when the user has held down Escape.
  void HandleUserHeldEscape();

  // Displays the exit instructions if the user presses escape rapidly.
  void ReShowExitBubbleIfNeeded();

  // Records the number of times the exit instructions were shown due to
  // repeated ESC keypresses.
  void RecordForcedBubbleReshowsHistogram();

  // Called after the bubble is hidden in tests, if set.
  ExclusiveAccessBubbleHideCallbackForTest bubble_hide_callback_for_test_;

  // Called after the esc repeat threshold is reached, if set.
  base::OnceClosure esc_repeat_triggered_for_test_;

  // Tracks the count of bubble reshows due to repeated ESC key presses.
  int forced_reshow_count_ = 0;

  KeyboardLockState keyboard_lock_state_ = KeyboardLockState::kUnlocked;
  base::OneShotTimer hold_timer_;

  // Window which determines whether to reshow the exit fullscreen instructions.
  base::TimeDelta esc_repeat_window_;

  const base::TickClock* esc_repeat_tick_clock_ = nullptr;

  base::circular_deque<base::TimeTicks> esc_keypress_tracker_;

  DISALLOW_COPY_AND_ASSIGN(KeyboardLockController);
};

#endif  //  CHROME_BROWSER_UI_EXCLUSIVE_ACCESS_KEYBOARD_LOCK_CONTROLLER_H_
