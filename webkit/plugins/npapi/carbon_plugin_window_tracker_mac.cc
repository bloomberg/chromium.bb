// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/plugins/npapi/carbon_plugin_window_tracker_mac.h"

#include "base/logging.h"

namespace webkit {
namespace npapi {

CarbonPluginWindowTracker::CarbonPluginWindowTracker() {}

CarbonPluginWindowTracker::~CarbonPluginWindowTracker() {}

CarbonPluginWindowTracker* CarbonPluginWindowTracker::SharedInstance() {
  static CarbonPluginWindowTracker* tracker = new CarbonPluginWindowTracker();
  return tracker;
}

WindowRef CarbonPluginWindowTracker::CreateDummyWindowForDelegate(
    OpaquePluginRef delegate) {
  // The real size will be set by the plugin instance, once that size is known.
  Rect window_bounds = { 0, 0, 100, 100 };
  WindowRef new_ref = NULL;
  if (CreateNewWindow(kDocumentWindowClass,
                      kWindowNoTitleBarAttribute,
                      &window_bounds,
                      &new_ref) == noErr) {
    window_to_delegate_map_[new_ref] = delegate;
    delegate_to_window_map_[delegate] = new_ref;
  }
  return new_ref;
}

OpaquePluginRef CarbonPluginWindowTracker::GetDelegateForDummyWindow(
    WindowRef window) const {
  WindowToDelegateMap::const_iterator i = window_to_delegate_map_.find(window);
  if (i != window_to_delegate_map_.end())
    return i->second;
  return NULL;
}

WindowRef CarbonPluginWindowTracker::GetDummyWindowForDelegate(
    OpaquePluginRef delegate) const {
  DelegateToWindowMap::const_iterator i =
      delegate_to_window_map_.find(delegate);
  if (i != delegate_to_window_map_.end())
    return i->second;
  return NULL;
}

void CarbonPluginWindowTracker::DestroyDummyWindowForDelegate(
    OpaquePluginRef delegate, WindowRef window) {
  DCHECK(GetDelegateForDummyWindow(window) == delegate);
  window_to_delegate_map_.erase(window);
  delegate_to_window_map_.erase(delegate);
  if (window)  // Check just in case the initial window creation failed.
    DisposeWindow(window);
}

}  // namespace npapi
}  // namespace webkit
