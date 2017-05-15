// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ui/ws/cursor_state.h"

#include "base/memory/ptr_util.h"
#include "services/ui/ws/display.h"
#include "services/ui/ws/display_manager.h"
#include "ui/base/cursor/cursor.h"

namespace ui {
namespace ws {

class CursorState::StateSnapshot {
 public:
  StateSnapshot()
      : cursor_data_(ui::CursorData(ui::CursorType::kNull)), visible_(true) {}
  StateSnapshot(const StateSnapshot& rhs) = default;
  ~StateSnapshot() {}

  const base::Optional<ui::CursorData>& global_override_cursor() const {
    return global_override_cursor_;
  }
  void SetGlobalOverrideCursor(const base::Optional<ui::CursorData>& cursor) {
    global_override_cursor_ = cursor;
  }

  const ui::CursorData& cursor_data() const { return cursor_data_; }
  void SetCursorData(const ui::CursorData& data) { cursor_data_ = data; }

  bool visible() const { return visible_; }
  void set_visible(bool visible) { visible_ = visible; }

 private:
  // An optional cursor set by the window manager which overrides per-window
  // requests.
  base::Optional<ui::CursorData> global_override_cursor_;

  // The last cursor set. Used to track whether we need to change the cursor.
  ui::CursorData cursor_data_;

  // Whether the cursor is visible.
  bool visible_;
};

CursorState::CursorState(DisplayManager* display_manager)
    : display_manager_(display_manager),
      current_state_(base::MakeUnique<StateSnapshot>()),
      state_on_unlock_(base::MakeUnique<StateSnapshot>()) {}

CursorState::~CursorState() {}

void CursorState::SetCurrentWindowCursor(const ui::CursorData& cursor) {
  if (!state_on_unlock_->cursor_data().IsSameAs(cursor))
    state_on_unlock_->SetCursorData(cursor);

  if (cursor_lock_count_ == 0 &&
      !current_state_->cursor_data().IsSameAs(cursor)) {
    current_state_->SetCursorData(cursor);
    SetPlatformCursor();
  }
}

void CursorState::LockCursor() {
  cursor_lock_count_++;
}

void CursorState::UnlockCursor() {
  cursor_lock_count_--;
  DCHECK_GE(cursor_lock_count_, 0);
  if (cursor_lock_count_ > 0)
    return;

  *current_state_ = *state_on_unlock_;
  SetPlatformCursor();
}

void CursorState::SetCursorVisible(bool visible) {
  state_on_unlock_->set_visible(visible);
  if (cursor_lock_count_ == 0 &&
      current_state_->visible() != state_on_unlock_->visible()) {
    current_state_->set_visible(visible);
    SetPlatformCursor();
  }
}

void CursorState::SetGlobalOverrideCursor(
    const base::Optional<ui::CursorData>& cursor) {
  state_on_unlock_->SetGlobalOverrideCursor(cursor);
  if (cursor_lock_count_ == 0) {
    current_state_->SetGlobalOverrideCursor(cursor);
    SetPlatformCursor();
  }
}

void CursorState::SetPlatformCursor() {
  DisplayManager* manager = display_manager_;
  auto set_on_all = [manager](const ui::CursorData& cursor) {
    for (Display* display : manager->displays())
      display->SetNativeCursor(cursor);
  };

  if (current_state_->visible()) {
    if (current_state_->global_override_cursor().has_value())
      set_on_all(current_state_->global_override_cursor().value());
    else
      set_on_all(current_state_->cursor_data());
  } else {
    set_on_all(ui::CursorData(ui::CursorType::kNone));
  }
}

}  // namespace ws
}  // namespace ui
