// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/dri/dri_window_manager.h"

namespace ui {

DriWindowManager::DriWindowManager() : last_allocated_widget_(0) {
}

DriWindowManager::~DriWindowManager() {
  DCHECK(delegate_map_.empty());
}

gfx::AcceleratedWidget DriWindowManager::NextAcceleratedWidget() {
  // We're not using 0 since other code assumes that a 0 AcceleratedWidget is an
  // invalid widget.
  return ++last_allocated_widget_;
}

void DriWindowManager::AddWindowDelegate(gfx::AcceleratedWidget widget,
                                         DriWindowDelegate* delegate) {
  DCHECK(delegate_map_.find(widget) == delegate_map_.end())
      << "Window delegate already added.";
  delegate_map_.insert(std::make_pair(widget, delegate));
}

void DriWindowManager::RemoveWindowDelegate(gfx::AcceleratedWidget widget) {
  WidgetToDelegateMap::iterator it = delegate_map_.find(widget);
  DCHECK(it != delegate_map_.end())
      << "Attempting to remove non-existing delegate.";

  delegate_map_.erase(it);
}

DriWindowDelegate* DriWindowManager::GetWindowDelegate(
    gfx::AcceleratedWidget widget) {
  WidgetToDelegateMap::iterator it = delegate_map_.find(widget);
  if (it != delegate_map_.end())
    return it->second;

  NOTREACHED();
  return NULL;
}

bool DriWindowManager::HasWindowDelegate(gfx::AcceleratedWidget widget) {
  return delegate_map_.find(widget) != delegate_map_.end();
}

}  // namespace ui
