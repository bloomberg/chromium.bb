// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_INFOBARS_BANNERS_INFOBAR_BANNER_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_INFOBARS_BANNERS_INFOBAR_BANNER_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

@protocol InfobarBannerDelegate;

// ViewController that manages an InfobarBanner. It consists of a leading icon,
// a title and optional subtitle, and a trailing button.
@interface InfobarBannerViewController : UIViewController

- (instancetype)initWithDelegate:(id<InfobarBannerDelegate>)delegate
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithNibName:(NSString*)nibNameOrNil
                         bundle:(NSBundle*)nibBundleOrNil NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)aDecoder NS_UNAVAILABLE;
- (instancetype)init NS_UNAVAILABLE;

// The icon displayed by this InfobarBanner.
@property(nonatomic, strong) UIImage* iconImage;

// The title displayed by this InfobarBanner.
@property(nonatomic, copy) NSString* titleText;

// The subtitle displayed by this InfobarBanner.
@property(nonatomic, copy) NSString* subTitleText;

// The button text displayed by this InfobarBanner.
@property(nonatomic, copy) NSString* buttonText;

@end

#endif  // IOS_CHROME_BROWSER_UI_INFOBARS_BANNERS_INFOBAR_BANNER_VIEW_CONTROLLER_H_
