// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INFOBARS_INFOBAR_CONTROLLER_H_
#define IOS_CHROME_BROWSER_INFOBARS_INFOBAR_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/infobars/infobar_ui_delegate.h"

namespace infobars {
class InfoBarDelegate;
}  // namespace infobars

// InfoBar for iOS acts as a UIViewController for InfoBarView.
@interface InfoBarController : NSObject <InfobarUIDelegate>

@property(nonatomic, readonly)
    infobars::InfoBarDelegate* infoBarDelegate;  // weak

- (instancetype)init NS_UNAVAILABLE;

- (instancetype)initWithInfoBarDelegate:
    (infobars::InfoBarDelegate*)infoBarDelegate NS_DESIGNATED_INITIALIZER;

@end

#endif  // IOS_CHROME_BROWSER_INFOBARS_INFOBAR_CONTROLLER_H_
