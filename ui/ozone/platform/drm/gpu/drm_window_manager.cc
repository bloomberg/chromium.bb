// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/drm/gpu/drm_window_manager.h"

#include "ui/ozone/platform/drm/gpu/drm_window.h"

namespace ui {

DrmWindowManager::DrmWindowManager() {
}

DrmWindowManager::~DrmWindowManager() {
  DCHECK(delegate_map_.empty());
}

void DrmWindowManager::AddWindowDelegate(gfx::AcceleratedWidget widget,
                                         scoped_ptr<DrmWindow> delegate) {
  std::pair<WidgetToDelegateMap::iterator, bool> result =
      delegate_map_.add(widget, delegate.Pass());
  DCHECK(result.second) << "Delegate already added.";
}

scoped_ptr<DrmWindow> DrmWindowManager::RemoveWindowDelegate(
    gfx::AcceleratedWidget widget) {
  scoped_ptr<DrmWindow> delegate = delegate_map_.take_and_erase(widget);
  DCHECK(delegate) << "Attempting to remove non-existing delegate for "
                   << widget;
  return delegate.Pass();
}

DrmWindow* DrmWindowManager::GetWindowDelegate(gfx::AcceleratedWidget widget) {
  WidgetToDelegateMap::iterator it = delegate_map_.find(widget);
  if (it != delegate_map_.end())
    return it->second;

  NOTREACHED() << "Attempting to get non-existing delegate for " << widget;
  return NULL;
}

}  // namespace ui
