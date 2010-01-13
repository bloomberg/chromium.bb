// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "webkit/glue/plugins/fake_plugin_window_tracker_mac.h"

FakePluginWindowTracker::FakePluginWindowTracker()
    : window_to_delegate_map_(CFDictionaryCreateMutable(kCFAllocatorDefault,
                                                        0, NULL, NULL)) {
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
    CFDictionaryAddValue(window_to_delegate_map_, new_ref, delegate);
  }
  return new_ref;
}

const WebPluginDelegateImpl* FakePluginWindowTracker::GetDelegateForFakeWindow(
    WindowRef window) const {
  return static_cast<const WebPluginDelegateImpl*>(
      CFDictionaryGetValue(window_to_delegate_map_, window));
}

void FakePluginWindowTracker::RemoveFakeWindowForDelegate(
    WebPluginDelegateImpl* delegate, WindowRef window) {
  DCHECK(GetDelegateForFakeWindow(window) == delegate);
  CFDictionaryRemoveValue(window_to_delegate_map_, delegate);
  if (window)  // Check just in case the initial window creation failed.
    DisposeWindow(window);
}

WindowRef FakePluginWindowTracker::get_active_plugin_window() {
  return active_plugin_window_;
}

void FakePluginWindowTracker::set_active_plugin_window(WindowRef window) {
  active_plugin_window_ = window;
}
