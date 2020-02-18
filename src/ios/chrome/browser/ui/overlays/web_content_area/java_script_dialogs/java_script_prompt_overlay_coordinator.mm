// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/overlays/web_content_area/java_script_dialogs/java_script_prompt_overlay_coordinator.h"

#import "ios/chrome/browser/overlays/public/overlay_request.h"
#import "ios/chrome/browser/overlays/public/web_content_area/java_script_prompt_overlay.h"
#import "ios/chrome/browser/ui/alert_view_controller/alert_view_controller.h"
#import "ios/chrome/browser/ui/overlays/web_content_area/java_script_dialogs/java_script_dialog_overlay_coordinator+subclassing.h"
#import "ios/chrome/browser/ui/overlays/web_content_area/java_script_dialogs/java_script_prompt_overlay_mediator.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface JavaScriptPromptOverlayCoordinator () <
    JavaScriptPromptOverlayMediatorDataSource>

// Returns the prompt configuration from the OverlayRequest.
@property(nonatomic, readonly)
    JavaScriptPromptOverlayRequestConfig* promptConfig;

@end

@implementation JavaScriptPromptOverlayCoordinator

#pragma mark - OverlayCoordinator

+ (BOOL)supportsRequest:(OverlayRequest*)request {
  return !!request->GetConfig<JavaScriptPromptOverlayRequestConfig>();
}

#pragma mark - JavaScriptPromptOverlayMediatorDataSource

- (NSString*)promptInputForMediator:(JavaScriptPromptOverlayMediator*)mediator {
  return self.alertViewController.textFieldResults.firstObject;
}

@end

@implementation JavaScriptPromptOverlayCoordinator (Subclassing)

- (JavaScriptDialogOverlayMediator*)newMediator {
  JavaScriptPromptOverlayMediator* mediator =
      [[JavaScriptPromptOverlayMediator alloc] initWithRequest:self.request];
  mediator.dataSource = self;
  return mediator;
}

@end
