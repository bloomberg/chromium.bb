// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/omnibox/omnibox_consumer.h"
#import "ios/chrome/browser/ui/omnibox/omnibox_text_field_ios.h"
#import "ios/chrome/browser/ui/orchestrator/edit_view_animatee.h"
#import "ios/chrome/browser/ui/orchestrator/location_bar_offset_provider.h"

@protocol LoadQueryCommands;
@protocol OmniboxFocuser;

// The view controller managing the omnibox textfield and its container view.
@interface OmniboxViewController : UIViewController<EditViewAnimatee,
                                                    LocationBarOffsetProvider,
                                                    OmniboxConsumer>

// The textfield used by this view controller.
@property(nonatomic, readonly, strong) OmniboxTextFieldIOS* textField;

// The default leading image to be used on omnibox focus before this is updated
// via OmniboxConsumer protocol.
@property(nonatomic, strong) UIImage* defaultLeadingImage;

// The default leading image to be used whenever the omnibox text is empty.
@property(nonatomic, strong) UIImage* emptyTextLeadingImage;

// Designated initializer.
- (instancetype)initWithIncognito:(BOOL)isIncognito;

// The dispatcher for the paste and go action.
@property(nonatomic, weak) id<LoadQueryCommands, OmniboxFocuser> dispatcher;

@end

#endif  // IOS_CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_VIEW_CONTROLLER_H_
