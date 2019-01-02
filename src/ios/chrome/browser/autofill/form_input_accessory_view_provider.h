// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_FORM_INPUT_ACCESSORY_VIEW_PROVIDER_H_
#define IOS_CHROME_BROWSER_AUTOFILL_FORM_INPUT_ACCESSORY_VIEW_PROVIDER_H_

#import <UIKit/UIKit.h>

namespace web {
struct FormActivityParams;
class WebState;
}  // namespace web

@protocol FormInputAccessoryViewDelegate;
@protocol FormInputAccessoryViewProvider;

// Block type to provide an accessory view asynchronously.
typedef void (^AccessoryViewReadyCompletion)(
    UIView* view,
    id<FormInputAccessoryViewProvider> provider);

// Represents an object that can provide a custom keyboard input accessory view.
@protocol FormInputAccessoryViewProvider<NSObject>

// A delegate for form navigation.
@property(nonatomic, assign) id<FormInputAccessoryViewDelegate>
    accessoryViewDelegate;

// Asynchronously retrieves an accessory view from this provider for the
// specified form/field and returns it via |accessoryViewUpdateBlock|. View
// will be nil if no accessories are available from this provider.
- (void)retrieveAccessoryViewForForm:(const web::FormActivityParams&)params
                            webState:(web::WebState*)webState
            accessoryViewUpdateBlock:
                (AccessoryViewReadyCompletion)accessoryViewUpdateBlock;

// Notifies this provider that the accessory view is going away.
- (void)inputAccessoryViewControllerDidReset;

@end

#endif  // IOS_CHROME_BROWSER_AUTOFILL_FORM_INPUT_ACCESSORY_VIEW_PROVIDER_H_
