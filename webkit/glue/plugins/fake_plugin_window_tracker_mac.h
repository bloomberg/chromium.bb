// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_PLUGINS_FAKE_PLUGIN_WINDOW_TRACKER_MAC_H_
#define WEBKIT_GLUE_PLUGINS_FAKE_PLUGIN_WINDOW_TRACKER_MAC_H_

#include <Carbon/Carbon.h>

#include "base/basictypes.h"
#include "base/scoped_cftyperef.h"

class ScopedActivePluginWindow;
class WebPluginDelegateImpl;

// Serves as a bridge between plugin delegate instances and the Carbon
// interposing library. The Carbon functions we interpose work in terms of
// WindowRefs, and we need to be able to map from those back to the plugin
// delegates that know what we should claim about the state of the world.
class __attribute__((visibility("default"))) FakePluginWindowTracker {
 public:
  FakePluginWindowTracker();

  // Returns the shared window tracker instance.
  static FakePluginWindowTracker* SharedInstance();

  // Creates a new fake window ref associated with |delegate|.
  WindowRef GenerateFakeWindowForDelegate(WebPluginDelegateImpl* delegate);

  // Returns the WebPluginDelegate associated with the given fake window ref.
  const WebPluginDelegateImpl* GetDelegateForFakeWindow(WindowRef window) const;

  // Removes the fake window ref entry for |delegate|.
  void RemoveFakeWindowForDelegate(WebPluginDelegateImpl* delegate,
                                   WindowRef window);

  // Gets the window for the plugin that is currently handling an input event.
  // To set the value, use ScopedActivePluginWindow.
  WindowRef get_active_plugin_window();

 private:
  friend class ScopedActivePluginWindow;
  // Sets the window corresponding to the plugin that is currently handling an
  // input event.
  void set_active_plugin_window(WindowRef window);

  scoped_cftyperef<CFMutableDictionaryRef> window_to_delegate_map_;

  WindowRef active_plugin_window_;  // weak reference

  DISALLOW_COPY_AND_ASSIGN(FakePluginWindowTracker);
};

// Helper to simplify correct usage of set_active_plugin_window.
// Instantiating will set the shared plugin window tracker's active window to
// |window| for the lifetime of the object, then NULL when it goes out of scope.
class ScopedActivePluginWindow {
 public:
  explicit ScopedActivePluginWindow(WindowRef window) : window_(window) {
    FakePluginWindowTracker::SharedInstance()->set_active_plugin_window(
        window_);
  }
  ~ScopedActivePluginWindow() {
    FakePluginWindowTracker::SharedInstance()->set_active_plugin_window(
        NULL);
  }
private:
  WindowRef window_;  // weak ref
  DISALLOW_COPY_AND_ASSIGN(ScopedActivePluginWindow);
};

#endif  // WEBKIT_GLUE_PLUGINS_FAKE_PLUGIN_WINDOW_TRACKER_MAC_H_
