// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "webkit/glue/plugins/fake_plugin_window_tracker_mac.h"

FakePluginWindowTracker::FakePluginWindowTracker() {
}

FakePluginWindowTracker* FakePluginWindowTracker::SharedInstance() {
  static FakePluginWindowTracker* tracker = new FakePluginWindowTracker();
  return tracker;
}

WindowRef FakePluginWindowTracker::GenerateFakeWindowForDelegate(
    WebPluginDelegateImpl* delegate) {
  // TODO(stuartmorgan): Eventually we will be interposing enough that we don't
  // even need a window, and can just use made-up identifiers, but for now we
  // create one so that things that we don't interpose won't crash trying to use
  // a bad WindowRef.

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

WebPluginDelegateImpl* FakePluginWindowTracker::GetDelegateForFakeWindow(
    WindowRef window) const {
  WindowToDelegateMap::const_iterator i = window_to_delegate_map_.find(window);
  if (i != window_to_delegate_map_.end())
    return i->second;
  return NULL;
}

WindowRef FakePluginWindowTracker::GetFakeWindowForDelegate(
    WebPluginDelegateImpl* delegate) const {
  DelegateToWindowMap::const_iterator i =
      delegate_to_window_map_.find(delegate);
  if (i != delegate_to_window_map_.end())
    return i->second;
  return NULL;
}

void FakePluginWindowTracker::RemoveFakeWindowForDelegate(
    WebPluginDelegateImpl* delegate, WindowRef window) {
  DCHECK(GetDelegateForFakeWindow(window) == delegate);
  window_to_delegate_map_.erase(window);
  delegate_to_window_map_.erase(delegate);
  if (window)  // Check just in case the initial window creation failed.
    DisposeWindow(window);
}
