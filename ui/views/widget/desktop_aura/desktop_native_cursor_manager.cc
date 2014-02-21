// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/widget/desktop_aura/desktop_native_cursor_manager.h"

#include "ui/aura/root_window.h"
#include "ui/base/cursor/cursor_loader.h"
#include "ui/views/widget/desktop_aura/desktop_cursor_loader_updater.h"

namespace views {

DesktopNativeCursorManager::DesktopNativeCursorManager(
    scoped_ptr<DesktopCursorLoaderUpdater> cursor_loader_updater)
    : cursor_loader_updater_(cursor_loader_updater.Pass()),
      cursor_loader_(ui::CursorLoader::Create()) {
  if (cursor_loader_updater_.get())
    cursor_loader_updater_->OnCreate(1.0f, cursor_loader_.get());
}

DesktopNativeCursorManager::~DesktopNativeCursorManager() {
}

gfx::NativeCursor DesktopNativeCursorManager::GetInitializedCursor(int type) {
  gfx::NativeCursor cursor(type);
  cursor_loader_->SetPlatformCursor(&cursor);
  return cursor;
}

void DesktopNativeCursorManager::AddRootWindow(
    aura::WindowEventDispatcher* dispatcher) {
  dispatchers_.insert(dispatcher);
}

void DesktopNativeCursorManager::RemoveRootWindow(
    aura::WindowEventDispatcher* dispatcher) {
  dispatchers_.erase(dispatcher);
}

void DesktopNativeCursorManager::SetDisplay(
    const gfx::Display& display,
    views::corewm::NativeCursorManagerDelegate* delegate) {
  cursor_loader_->UnloadAll();
  cursor_loader_->set_display(display);

  if (cursor_loader_updater_.get())
    cursor_loader_updater_->OnDisplayUpdated(display, cursor_loader_.get());

  SetCursor(delegate->GetCursor(), delegate);
}

void DesktopNativeCursorManager::SetCursor(
    gfx::NativeCursor cursor,
    views::corewm::NativeCursorManagerDelegate* delegate) {
  gfx::NativeCursor new_cursor = cursor;
  cursor_loader_->SetPlatformCursor(&new_cursor);
  delegate->CommitCursor(new_cursor);

  if (delegate->IsCursorVisible()) {
    for (Dispatchers::const_iterator i = dispatchers_.begin();
         i != dispatchers_.end();
         ++i) {
      (*i)->host()->SetCursor(new_cursor);
    }
  }
}

void DesktopNativeCursorManager::SetVisibility(
    bool visible,
    views::corewm::NativeCursorManagerDelegate* delegate) {
  delegate->CommitVisibility(visible);

  if (visible) {
    SetCursor(delegate->GetCursor(), delegate);
  } else {
    gfx::NativeCursor invisible_cursor(ui::kCursorNone);
    cursor_loader_->SetPlatformCursor(&invisible_cursor);
    for (Dispatchers::const_iterator i = dispatchers_.begin();
         i != dispatchers_.end();
         ++i) {
      (*i)->host()->SetCursor(invisible_cursor);
    }
  }

  for (Dispatchers::const_iterator i = dispatchers_.begin();
       i != dispatchers_.end();
       ++i) {
    (*i)->host()->OnCursorVisibilityChanged(visible);
  }
}

void DesktopNativeCursorManager::SetCursorSet(
    ui::CursorSetType cursor_set,
    views::corewm::NativeCursorManagerDelegate* delegate) {
  NOTIMPLEMENTED();
}

void DesktopNativeCursorManager::SetScale(
    float scale,
    views::corewm::NativeCursorManagerDelegate* delegate) {
  NOTIMPLEMENTED();
}

void DesktopNativeCursorManager::SetMouseEventsEnabled(
    bool enabled,
    views::corewm::NativeCursorManagerDelegate* delegate) {
  delegate->CommitMouseEventsEnabled(enabled);

  // TODO(erg): In the ash version, we set the last mouse location on Env. I'm
  // not sure this concept makes sense on the desktop.

  SetVisibility(delegate->IsCursorVisible(), delegate);

  for (Dispatchers::const_iterator i = dispatchers_.begin();
       i != dispatchers_.end();
       ++i) {
    (*i)->OnMouseEventsEnableStateChanged(enabled);
  }
}

}  // namespace views
