// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_OVERLAYS_WEB_CONTENT_AREA_JAVA_SCRIPT_DIALOGS_JAVA_SCRIPT_DIALOG_OVERLAY_COORDINATOR_SUBCLASSING_H_
#define IOS_CHROME_BROWSER_UI_OVERLAYS_WEB_CONTENT_AREA_JAVA_SCRIPT_DIALOGS_JAVA_SCRIPT_DIALOG_OVERLAY_COORDINATOR_SUBCLASSING_H_

#import "ios/chrome/browser/ui/overlays/web_content_area/java_script_dialogs/java_script_dialog_overlay_coordinator.h"

@class AlertViewController;
@class JavaScriptDialogOverlayMediator;

// Category that allows subclasses to update UI using the OverlayRequest
// configuration.
@interface JavaScriptDialogOverlayCoordinator (Subclassing)

// The alert used to show the dialog UI.
@property(nonatomic, readonly) AlertViewController* alertViewController;

// Subclasses must override this selector to create a mediator that will
// configure the alert using the OverlayRequest's config.
- (JavaScriptDialogOverlayMediator*)newMediator;

@end

#endif  // IOS_CHROME_BROWSER_UI_OVERLAYS_WEB_CONTENT_AREA_JAVA_SCRIPT_DIALOGS_JAVA_SCRIPT_DIALOG_OVERLAY_COORDINATOR_SUBCLASSING_H_
