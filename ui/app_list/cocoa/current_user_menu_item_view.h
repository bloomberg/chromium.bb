// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_APP_LIST_COCOA_CURRENT_USER_MENU_ITEM_VIEW_H_
#define UI_APP_LIST_COCOA_CURRENT_USER_MENU_ITEM_VIEW_H_

#import <Cocoa/Cocoa.h>

#include "ui/app_list/app_list_export.h"

namespace app_list {
class AppListViewDelegate;
}

// The custom in-menu view representing the currently signed-in user.
APP_LIST_EXPORT
@interface CurrentUserMenuItemView : NSView

- (id)initWithDelegate:(app_list::AppListViewDelegate*)delegate;

@end

#endif  // UI_APP_LIST_COCOA_CURRENT_USER_MENU_ITEM_VIEW_H_
