// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#include "base/memory/scoped_ptr.h"

namespace app_list {
class AppListModel;
class AppListViewDelegate;
}

// AppListView is the top-level view and controller of app list UI.
@interface AppListView : NSView {
 @private
  scoped_ptr<app_list::AppListModel> model_;
  scoped_ptr<app_list::AppListViewDelegate> delegate_;
}

// Takes ownership of |appListViewDelegate|.
- (id)initWithViewDelegate:(app_list::AppListViewDelegate*)appListViewDelegate;

@end
