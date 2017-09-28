// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/ios/app/view_utils.h"

namespace remoting {

UIViewController* TopPresentingVC() {
  UIViewController* topController =
      UIApplication.sharedApplication.keyWindow.rootViewController;

  while (topController.presentedViewController) {
    topController = topController.presentedViewController;
  }

  return topController;
}

UILayoutGuide* SafeAreaLayoutGuideForView(UIView* view) {
  if (@available(iOS 11, *)) {
    return view.safeAreaLayoutGuide;
  } else {
    NSString* kChromeSafeAreaLayoutGuideShim =
        @"ChromotingSafeAreaLayoutGuideShim";
    // Search for an existing shim safe area layout guide:
    for (UILayoutGuide* guide in view.layoutGuides) {
      if ([guide.identifier isEqualToString:kChromeSafeAreaLayoutGuideShim]) {
        return guide;
      }
    }
    // If no existing shim exist, create and return a new one.
    UILayoutGuide* safeAreaLayoutShim = [[UILayoutGuide alloc] init];
    safeAreaLayoutShim.identifier = kChromeSafeAreaLayoutGuideShim;
    [view addLayoutGuide:safeAreaLayoutShim];
    [NSLayoutConstraint activateConstraints:@[
      [safeAreaLayoutShim.leadingAnchor
          constraintEqualToAnchor:view.leadingAnchor],
      [safeAreaLayoutShim.trailingAnchor
          constraintEqualToAnchor:view.trailingAnchor],
      [safeAreaLayoutShim.topAnchor constraintEqualToAnchor:view.topAnchor],
      [safeAreaLayoutShim.bottomAnchor
          constraintEqualToAnchor:view.bottomAnchor]
    ]];
    return safeAreaLayoutShim;
  }
}

void SetAccessibilityInfoFromImage(UIBarButtonItem* button) {
  button.accessibilityLabel = button.image.accessibilityLabel;
}

void SetAccessibilityInfoFromImage(UIButton* button) {
  button.accessibilityLabel =
      [button imageForState:UIControlStateNormal].accessibilityLabel;
}

}  // namespace remoting
