// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/menu/menu_cocoa_watcher_mac.h"

#import <Cocoa/Cocoa.h>

#import <utility>

namespace views {

MenuCocoaWatcherMac::MenuCocoaWatcherMac(base::OnceClosure callback)
    : callback_(std::move(callback)) {
  observer_token_ = [[NSNotificationCenter defaultCenter]
      addObserverForName:NSMenuDidBeginTrackingNotification
                  object:[NSApp mainMenu]
                   queue:nil
              usingBlock:^(NSNotification* notification) {
                std::move(this->callback_).Run();
              }];
}

MenuCocoaWatcherMac::~MenuCocoaWatcherMac() {
  [[NSNotificationCenter defaultCenter] removeObserver:observer_token_];
}

}  // namespace views
