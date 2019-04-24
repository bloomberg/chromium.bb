// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTOFILL_MANUAL_FILL_PASSWORD_MEDIATOR_H_
#define IOS_CHROME_BROWSER_UI_AUTOFILL_MANUAL_FILL_PASSWORD_MEDIATOR_H_

#import <UIKit/UIKit.h>

#include "base/memory/ref_counted.h"

@protocol ManualFillContentDelegate;
@protocol ManualFillPasswordConsumer;
@protocol PasswordListNavigator;

namespace password_manager {
class PasswordStore;
}  // namespace password_manager

class GURL;

class WebStateList;

namespace manual_fill {

extern NSString* const ManagePasswordsAccessibilityIdentifier;
extern NSString* const OtherPasswordsAccessibilityIdentifier;
extern NSString* const SuggestPasswordAccessibilityIdentifier;

}  // namespace manual_fill

// Object in charge of getting the passwords relevant for the manual fill
// passwords UI.
@interface ManualFillPasswordMediator : NSObject<UISearchResultsUpdating>

// The consumer for passwords updates. Setting it will trigger the consumer
// methods with the current data.
@property(nonatomic, weak) id<ManualFillPasswordConsumer> consumer;
// The delegate in charge of using the content selected by the user.
@property(nonatomic, weak) id<ManualFillContentDelegate> contentDelegate;
// The object in charge of navigation.
@property(nonatomic, weak) id<PasswordListNavigator> navigator;
// If YES  actions will be post to the consumer. Set this value before
// setting the consumer, since just setting this won't trigger the consumer
// callbacks. Defaults to NO.
@property(nonatomic, assign, getter=isActionSectionEnabled)
    BOOL actionSectionEnabled;

// The designated initializer. |passwordStore| must not be nil.
- (instancetype)initWithPasswordStore:
    (scoped_refptr<password_manager::PasswordStore>)passwordStore
    NS_DESIGNATED_INITIALIZER;

// Unavailable. Use |initWithWebStateList:passwordStore:|.
- (instancetype)init NS_UNAVAILABLE;

// Fetches passwords using |origin| as the filter. If origin is empty (invalid)
// it will fetch all the passwords.
- (void)fetchPasswordsForURL:(const GURL&)URL;

@end

#endif  // IOS_CHROME_BROWSER_UI_AUTOFILL_MANUAL_FILL_PASSWORD_MEDIATOR_H_
