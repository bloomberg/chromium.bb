// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/dri/dri_window_manager.h"

#include "base/logging.h"
#include "ui/ozone/platform/dri/dri_window.h"

namespace ui {

DriWindowManager::DriWindowManager()
    : last_allocated_widget_(0), event_grabber_(gfx::kNullAcceleratedWidget) {
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
}

void DriWindowManager::RemoveWindow(gfx::AcceleratedWidget widget) {
  WidgetToWindowMap::iterator it = window_map_.find(widget);
  if (it != window_map_.end())
    window_map_.erase(it);
  else
    NOTREACHED() << "Attempting to remove non-existing window " << widget;

  if (event_grabber_ == widget)
    event_grabber_ = gfx::kNullAcceleratedWidget;
}

DriWindow* DriWindowManager::GetWindow(gfx::AcceleratedWidget widget) {
  WidgetToWindowMap::iterator it = window_map_.find(widget);
  if (it != window_map_.end())
    return it->second;

  NOTREACHED() << "Attempting to get non-existing window " << widget;
  return NULL;
}

DriWindow* DriWindowManager::GetWindowAt(const gfx::Point& location) {
  for (auto it = window_map_.begin(); it != window_map_.end(); ++it)
    if (it->second->GetBounds().Contains(location))
      return it->second;

  return NULL;
}

DriWindow* DriWindowManager::GetPrimaryWindow() {
  auto it = window_map_.begin();
  return it != window_map_.end() ? it->second : nullptr;
}

void DriWindowManager::GrabEvents(gfx::AcceleratedWidget widget) {
  if (event_grabber_ != gfx::kNullAcceleratedWidget)
    return;
  event_grabber_ = widget;
}

void DriWindowManager::UngrabEvents(gfx::AcceleratedWidget widget) {
  if (event_grabber_ != widget)
    return;
  event_grabber_ = gfx::kNullAcceleratedWidget;
}

}  // namespace ui
