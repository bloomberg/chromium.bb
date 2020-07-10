// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_DIALOGS_NON_MODAL_NON_MODAL_ALERT_PRESENTATION_UPDATER_H_
#define IOS_CHROME_BROWSER_UI_DIALOGS_NON_MODAL_NON_MODAL_ALERT_PRESENTATION_UPDATER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/fullscreen/fullscreen_ui_element.h"

// UIAlertController does not allow for customization of its presentation via
// UIPresentationControllers, UIViewControllerTransitioningDelegates, or
// UIModalPresentationStyle.  This helper object applies a mask to the
// presentation container view of a UIAlertController, and forwards touches to
// the presenting view controller when they lie outside of that mask.  The
// updater is a FullscreenUIElement, and should be notified of fullscreen UI
// changes.
//
// Example usage:
//
// UIAlertController* alert = [[UIAlertControler alloc] init...];
// [baseViewController presentViewController:alert animated...];
//
// NonModalAlertPresentationUpdater* presentationUpdater =
//     [[NonModalAlertPresentationUpdater alloc] initWithAlertController:alert];
// [presentationUpdater setUpNonModalPresentationWithViewportInsets:
//     fullscrenController->GetCurrentViewportInsets()];
//
// _fullscreenObserver =
//     std::make_unique<FullscreenUIUpdater>(_nonModalPresentationController);
// fullscreenController->AddObserver(_fullscreenObserver.get());
//
@interface NonModalAlertPresentationUpdater : NSObject <FullscreenUIElement>

// Designated initializer for a presentation updater that makes
// |alertController| non-modal.
- (instancetype)initWithAlertController:(UIAlertController*)alertController
    NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

// Updates the alert to be presented non-modally by applying a mask to its
// presentation container and forwarding touches outside that mask to its
// presenting view controller.  |viewportInsets| are the insets from the side of
// the window to mask the alert presentation container, and should be enough so
// that the toolbars are completely visible.
- (void)setUpNonModalPresentationWithViewportInsets:
    (UIEdgeInsets)viewportInsets;

@end

#endif  // IOS_CHROME_BROWSER_UI_DIALOGS_NON_MODAL_NON_MODAL_ALERT_PRESENTATION_UPDATER_H_
