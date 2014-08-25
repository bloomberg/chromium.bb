// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/dri/dri_window_manager.h"

#include "ui/ozone/platform/dri/dri_window_delegate.h"

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

void DriWindowManager::AddWindowDelegate(
    gfx::AcceleratedWidget widget,
    scoped_ptr<DriWindowDelegate> delegate) {
  std::pair<WidgetToDelegateMap::iterator, bool> result =
      delegate_map_.add(widget, delegate.Pass());
  DCHECK(result.second) << "Delegate already added.";
}

scoped_ptr<DriWindowDelegate> DriWindowManager::RemoveWindowDelegate(
    gfx::AcceleratedWidget widget) {
  scoped_ptr<DriWindowDelegate> delegate = delegate_map_.take_and_erase(widget);
  DCHECK(delegate) << "Attempting to remove non-existing delegate.";
  return delegate.Pass();
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
