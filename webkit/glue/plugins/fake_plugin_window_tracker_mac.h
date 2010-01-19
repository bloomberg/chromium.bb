// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_PLUGINS_FAKE_PLUGIN_WINDOW_TRACKER_MAC_H_
#define WEBKIT_GLUE_PLUGINS_FAKE_PLUGIN_WINDOW_TRACKER_MAC_H_

#include <Carbon/Carbon.h>
#include <map>

#include "base/basictypes.h"

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
  WebPluginDelegateImpl* GetDelegateForFakeWindow(WindowRef window) const;

  // Returns the fake window ref associated with |delegate|.
  WindowRef GetFakeWindowForDelegate(WebPluginDelegateImpl* delegate) const;

  // Removes the fake window ref entry for |delegate|.
  void RemoveFakeWindowForDelegate(WebPluginDelegateImpl* delegate,
                                   WindowRef window);

 private:
  typedef std::map<WindowRef, WebPluginDelegateImpl*> WindowToDelegateMap;
  typedef std::map<WebPluginDelegateImpl*, WindowRef> DelegateToWindowMap;
  WindowToDelegateMap window_to_delegate_map_;
  DelegateToWindowMap delegate_to_window_map_;

  DISALLOW_COPY_AND_ASSIGN(FakePluginWindowTracker);
};

#endif  // WEBKIT_GLUE_PLUGINS_FAKE_PLUGIN_WINDOW_TRACKER_MAC_H_
