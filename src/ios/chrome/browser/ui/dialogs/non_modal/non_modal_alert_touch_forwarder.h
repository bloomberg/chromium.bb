// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_DIALOGS_NON_MODAL_NON_MODAL_ALERT_TOUCH_FORWARDER_H_
#define IOS_CHROME_BROWSER_UI_DIALOGS_NON_MODAL_NON_MODAL_ALERT_TOUCH_FORWARDER_H_

#import <UIKit/UIKit.h>

// Helper view that is inserted into the UIAlertController's hierarchy that
// overrides |-hitTest:withEvent:| to forward touch events that lie outside of
// the non-modal alert presentation container mask.  This view should be
// inserted between the UIAlertController's view and its presentation container
// view.  This solution is necessary because UIAlertController presents a full-
// screen shim over the app that intercepts touches even if masked.
//
// Example Usage:
//
// # After adding mask, toolbars are visible but non-interactable.
// [viewController presentViewController:alertController ...];
// alertController.presentationController.containerView.layer.mask = mask;
//
// # Inserting |forwarder| into the hierarchy forwards touches to the presenter.
// NonModalAlertTouchForwarder* forwarder =
//     [[NonModalAlertTouchForwarder alloc] init];
// forwarder.mask = mask;
// forwarder.forwardingTarger = viewController.view;
// UIView* alertView = alertController.view;
// [alertView.superView insertSubview:forwarder belowSubview:alertView];
@interface NonModalAlertTouchForwarder : UIView

// The non-modal alert presentation container mask.
@property(nonatomic, weak) CALayer* mask;
// The UIView that should receive touches that occur outside of |mask|.
@property(nonatomic, weak) UIView* forwardingTarget;

@end

#endif  // IOS_CHROME_BROWSER_UI_DIALOGS_NON_MODAL_NON_MODAL_ALERT_TOUCH_FORWARDER_H_
