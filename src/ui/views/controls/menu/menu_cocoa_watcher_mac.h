// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_MENU_MENU_COCOA_WATCHER_MAC_H_
#define UI_VIEWS_CONTROLS_MENU_MENU_COCOA_WATCHER_MAC_H_

#include <objc/objc.h>

#include "base/callback.h"
#include "base/macros.h"
#include "ui/views/views_export.h"

namespace views {

// This class executes a callback when a native menu begins tracking. With
// native menus, each one automatically closes when a new one begins tracking.
// This allows Views menus to tie into this behavior.
class VIEWS_EXPORT MenuCocoaWatcherMac {
 public:
  explicit MenuCocoaWatcherMac(base::OnceClosure callback);
  ~MenuCocoaWatcherMac();

 private:
  // The closure to call when the notification comes in.
  base::OnceClosure callback_;

  // The token representing the notification observer.
  id observer_token_;

  DISALLOW_COPY_AND_ASSIGN(MenuCocoaWatcherMac);
};

}  // namespace views

#endif  // UI_VIEWS_CONTROLS_MENU_MENU_COCOA_WATCHER_MAC_H_
