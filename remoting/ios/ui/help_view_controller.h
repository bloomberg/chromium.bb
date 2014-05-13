// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_IOS_UI_HELP_VIEW_CONTROLLER_H_
#define REMOTING_IOS_UI_HELP_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

@interface HelpViewController : UIViewController {
 @private
  IBOutlet UIWebView* _webView;
}

@end

#endif  // REMOTING_IOS_UI_HELP_VIEW_CONTROLLER_H_