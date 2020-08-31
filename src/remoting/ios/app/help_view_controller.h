// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_IOS_APP_HELP_VIEW_CONTROLLER_H_
#define REMOTING_IOS_APP_HELP_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "remoting/ios/app/web_view_controller.h"

// A VC that shows the help center.
@interface HelpViewController : WebViewController

- (instancetype)init;

@end

#endif  // REMOTING_IOS_APP_HELP_VIEW_CONTROLLER_H_
