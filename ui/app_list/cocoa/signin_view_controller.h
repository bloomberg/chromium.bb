// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_APP_LIST_COCOA_APP_LIST_SIGNIN_VIEW_CONTROLLER_H_
#define UI_APP_LIST_COCOA_APP_LIST_SIGNIN_VIEW_CONTROLLER_H_

#import <Cocoa/Cocoa.h>

#include "ui/app_list/app_list_export.h"

namespace app_list {
class SigninDelegate;
}

// Controller for the app list signin page. The signin view shows a blue button
// that opens a browser window to conduct the signin flow. The delegate also
// provides UI text and actions for the view, including "Learn More" and
// "Settings" links.
APP_LIST_EXPORT
@interface SigninViewController : NSViewController {
 @private
  app_list::SigninDelegate* delegate_; // Weak. Owned by AppListViewDelegate.
}

- (id)initWithFrame:(NSRect)frame
       cornerRadius:(CGFloat)cornerRadius
           delegate:(app_list::SigninDelegate*)delegate;

@end

#endif  // UI_APP_LIST_COCOA_APP_LIST_SIGNIN_VIEW_CONTROLLER_H_
