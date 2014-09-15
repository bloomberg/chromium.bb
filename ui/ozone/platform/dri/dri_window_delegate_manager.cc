// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/dri/dri_window_delegate_manager.h"

#include "ui/ozone/platform/dri/dri_window_delegate.h"

namespace ui {

DriWindowDelegateManager::DriWindowDelegateManager() {
}

DriWindowDelegateManager::~DriWindowDelegateManager() {
  DCHECK(delegate_map_.empty());
}

void DriWindowDelegateManager::AddWindowDelegate(
    gfx::AcceleratedWidget widget,
    scoped_ptr<DriWindowDelegate> delegate) {
  std::pair<WidgetToDelegateMap::iterator, bool> result =
      delegate_map_.add(widget, delegate.Pass());
  DCHECK(result.second) << "Delegate already added.";
}

scoped_ptr<DriWindowDelegate> DriWindowDelegateManager::RemoveWindowDelegate(
    gfx::AcceleratedWidget widget) {
  scoped_ptr<DriWindowDelegate> delegate = delegate_map_.take_and_erase(widget);
  DCHECK(delegate) << "Attempting to remove non-existing delegate for "
                   << widget;
  return delegate.Pass();
}

DriWindowDelegate* DriWindowDelegateManager::GetWindowDelegate(
    gfx::AcceleratedWidget widget) {
  WidgetToDelegateMap::iterator it = delegate_map_.find(widget);
  if (it != delegate_map_.end())
    return it->second;

  NOTREACHED() << "Attempting to get non-existing delegate for " << widget;
  return NULL;
}

bool DriWindowDelegateManager::HasWindowDelegate(
    gfx::AcceleratedWidget widget) {
  return delegate_map_.find(widget) != delegate_map_.end();
}

}  // namespace ui
