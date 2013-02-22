// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_APP_LIST_COCOA_APP_LIST_WINDOW_CONTROLLER_H_
#define UI_APP_LIST_COCOA_APP_LIST_WINDOW_CONTROLLER_H_

#import <Cocoa/Cocoa.h>

#include "base/memory/scoped_nsobject.h"

@class AppsGridController;

// Controller for the app list NSWindow.
@interface AppListWindowController : NSWindowController<NSWindowDelegate> {
  scoped_nsobject<AppsGridController> appsGridController_;
}

- (id)initWithGridController:(AppsGridController*)gridController;

- (AppsGridController*)appsGridController;

@end

#endif  // UI_APP_LIST_COCOA_APP_LIST_WINDOW_CONTROLLER_H_
