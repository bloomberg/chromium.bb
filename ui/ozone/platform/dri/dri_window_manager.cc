// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/dri/dri_window_manager.h"

#include "base/logging.h"
#include "ui/ozone/platform/dri/dri_cursor.h"
#include "ui/ozone/platform/dri/dri_window.h"

namespace ui {

namespace {

gfx::PointF GetDefaultCursorLocation(DriWindow* window) {
  return gfx::PointF(window->GetBounds().width() / 2,
                     window->GetBounds().height() / 2);
}

}  // namespace

DriWindowManager::DriWindowManager(HardwareCursorDelegate* cursor_delegate)
    : last_allocated_widget_(0), cursor_(new DriCursor(cursor_delegate, this)) {
}

DriWindowManager::~DriWindowManager() {
}

gfx::AcceleratedWidget DriWindowManager::NextAcceleratedWidget() {
  // We're not using 0 since other code assumes that a 0 AcceleratedWidget is an
  // invalid widget.
  return ++last_allocated_widget_;
}

void DriWindowManager::AddWindow(gfx::AcceleratedWidget widget,
                                 DriWindow* window) {
  std::pair<WidgetToWindowMap::iterator, bool> result = window_map_.insert(
      std::pair<gfx::AcceleratedWidget, DriWindow*>(widget, window));
  DCHECK(result.second) << "Window for " << widget << " already added.";

  if (cursor_->GetCursorWindow() == gfx::kNullAcceleratedWidget)
    ResetCursorLocation();
}

void DriWindowManager::RemoveWindow(gfx::AcceleratedWidget widget) {
  WidgetToWindowMap::iterator it = window_map_.find(widget);
  if (it != window_map_.end())
    window_map_.erase(it);
  else
    NOTREACHED() << "Attempting to remove non-existing window " << widget;

  if (cursor_->GetCursorWindow() == widget)
    ResetCursorLocation();
}

DriWindow* DriWindowManager::GetWindow(gfx::AcceleratedWidget widget) {
  WidgetToWindowMap::iterator it = window_map_.find(widget);
  if (it != window_map_.end())
    return it->second;

  NOTREACHED() << "Attempting to get non-existing window " << widget;
  return NULL;
}

void DriWindowManager::ResetCursorLocation() {
  gfx::AcceleratedWidget cursor_widget = gfx::kNullAcceleratedWidget;
  gfx::PointF location;
  if (!window_map_.empty()) {
    WidgetToWindowMap::iterator it = window_map_.begin();
    cursor_widget = it->first;
    location = GetDefaultCursorLocation(it->second);
  }

  cursor_->MoveCursorTo(cursor_widget, location);
}

}  // namespace ui
