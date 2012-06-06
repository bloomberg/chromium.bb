// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_CURSOR_MANAGER_H_
#define UI_AURA_CURSOR_MANAGER_H_
#pragma once

#include "base/basictypes.h"
#include "ui/aura/aura_export.h"
#include "ui/gfx/native_widget_types.h"

namespace aura {
class CursorDelegate;

// This class controls the visibility and the type of the cursor.
// The cursor type can be locked so that the type stays the same
// until it's unlocked.
class AURA_EXPORT CursorManager {
 public:
  CursorManager();
  ~CursorManager();

  void set_delegate(CursorDelegate* delegate) { delegate_ = delegate; }

  // Locks/Unlocks the cursor change.
  void LockCursor();
  void UnlockCursor();

  void SetCursor(gfx::NativeCursor);

  // Shows or hides the cursor.
  void ShowCursor(bool show);
  bool cursor_visible() const { return cursor_visible_; }

 private:
  CursorDelegate* delegate_;

  // Number of times LockCursor() has been invoked without a corresponding
  // UnlockCursor().
  int cursor_lock_count_;

  // Set to true if UpdateCursor() is invoked while |cursor_lock_count_| == 0.
  bool did_cursor_change_;

  // Cursor to set once |cursor_lock_count_| is set to 0. Only valid if
  // |did_cursor_change_| is true.
  gfx::NativeCursor cursor_to_set_on_unlock_;

  // Is cursor visible?
  bool cursor_visible_;

  DISALLOW_COPY_AND_ASSIGN(CursorManager);
};

}  // namespace aura

#endif  // UI_AURA_CURSOR_MANAGER_H_

