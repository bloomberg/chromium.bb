// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_OMNIBOX_POPUP_SWIFTUI_PREVIEWS_OMNIBOX_POPUP_POPUP_MEDIATOR_H_
#define IOS_CHROME_BROWSER_UI_OMNIBOX_POPUP_SWIFTUI_PREVIEWS_OMNIBOX_POPUP_POPUP_MEDIATOR_H_

#import <Foundation/Foundation.h>

@class PopupModel;

@interface PopupMediator : NSObject

@property(nonatomic, strong, readonly) PopupModel* model;

@end

#endif  // IOS_CHROME_BROWSER_UI_OMNIBOX_POPUP_SWIFTUI_PREVIEWS_OMNIBOX_POPUP_POPUP_MEDIATOR_H_
