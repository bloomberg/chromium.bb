// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/cursor_manager.h"

#include "base/logging.h"
#include "ui/aura/cursor_delegate.h"
#include "ui/aura/env.h"

namespace aura {

CursorManager::CursorManager()
    : delegate_(NULL),
      cursor_lock_count_(0),
      did_cursor_change_(false),
      cursor_to_set_on_unlock_(0),
      cursor_visible_(true) {
}

CursorManager::~CursorManager() {
}

void CursorManager::LockCursor() {
  cursor_lock_count_++;
}

void CursorManager::UnlockCursor() {
  cursor_lock_count_--;
  DCHECK_GE(cursor_lock_count_, 0);
  if (cursor_lock_count_ == 0) {
    if (did_cursor_change_) {
      did_cursor_change_ = false;
      if (delegate_)
        delegate_->SetCursor(cursor_to_set_on_unlock_);
    }
    did_cursor_change_ = false;
    cursor_to_set_on_unlock_ = gfx::kNullCursor;
  }
}

void CursorManager::SetCursor(gfx::NativeCursor cursor) {
  if (cursor_lock_count_ == 0) {
    if (delegate_)
      delegate_->SetCursor(cursor);
  } else {
    cursor_to_set_on_unlock_ = cursor;
    did_cursor_change_ = true;
  }
}

void CursorManager::ShowCursor(bool show) {
  cursor_visible_ = show;
  if (delegate_)
    delegate_->ShowCursor(show);
}

}  // namespace aura
