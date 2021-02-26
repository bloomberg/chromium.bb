// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_COORDINATOR_H_

#import "ios/chrome/browser/ui/coordinators/chrome_coordinator.h"

class WebOmniboxEditController;
@protocol EditViewAnimatee;
@class OmniboxPopupCoordinator;
@class OmniboxTextFieldIOS;
@protocol LocationBarOffsetProvider;
@protocol OmniboxPopupPresenterDelegate;

// The coordinator for the omnibox.
@interface OmniboxCoordinator : ChromeCoordinator

// The edit controller interfacing the |textField| and the omnibox components
// code. Needs to be set before the coordinator is started.
@property(nonatomic, assign) WebOmniboxEditController* editController;
// Returns the animatee for the omnibox focus orchestrator.
@property(nonatomic, strong, readonly) id<EditViewAnimatee> animatee;

// The view controller managed by this coordinator. The parent of this
// coordinator is expected to add it to the responder chain.
- (UIViewController*)managedViewController;

// Offset provider for location bar animations.
- (id<LocationBarOffsetProvider>)offsetProvider;

// Start this coordinator. When it starts, it expects to have |textField| and
// |editController|.
- (void)start;
// Stop this coordinator.
- (void)stop;

// Indicates if the omnibox is the first responder.
- (BOOL)isOmniboxFirstResponder;
// Inserts text to the omnibox without triggering autocomplete.
// Use this method to insert target URL or search terms for alternative input
// methods, such as QR code scanner or voice search.
- (void)insertTextToOmnibox:(NSString*)string;
// Update the contents and the styling of the omnibox.
- (void)updateOmniboxState;
// Use this method to make the omnibox first responder.
- (void)focusOmnibox;

// Prepare the omnibox for scribbling.
- (void)focusOmniboxForScribble;
// Target input for scribble targeting the omnibox.
- (UIResponder<UITextInput>*)scribbleInput;

// Use this method to resign |textField| as the first responder.
- (void)endEditing;
// Creates a child popup coordinator. The popup coordinator is linked to the
// |textField| through components code.
- (OmniboxPopupCoordinator*)createPopupCoordinator:
    (id<OmniboxPopupPresenterDelegate>)presenterDelegate;

@end

#endif  // IOS_CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_COORDINATOR_H_
