// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/util/uikit_ui_util.h"

#import <Accelerate/Accelerate.h>
#import <Foundation/Foundation.h>
#import <QuartzCore/QuartzCore.h>
#import <UIKit/UIKit.h>
#include <stddef.h>
#include <stdint.h>
#include <cmath>

#include "base/check_op.h"
#include "base/ios/ios_util.h"
#include "base/mac/foundation_util.h"
#include "base/notreached.h"
#include "base/numerics/math_constants.h"
#include "ios/chrome/browser/system_flags.h"
#include "ios/chrome/browser/ui/ui_feature_flags.h"
#include "ios/chrome/browser/ui/util/dynamic_type_util.h"
#include "ios/chrome/browser/ui/util/rtl_geometry.h"
#include "ios/chrome/browser/ui/util/ui_util.h"
#include "ios/web/public/thread/web_thread.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/l10n_util_mac.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/ios/uikit_util.h"
#include "ui/gfx/scoped_cg_context_save_gstate_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

void SetA11yLabelAndUiAutomationName(
    NSObject<UIAccessibilityIdentification>* element,
    int idsAccessibilityLabel,
    NSString* englishUiAutomationName) {
  [element setAccessibilityLabel:l10n_util::GetNSString(idsAccessibilityLabel)];
  [element setAccessibilityIdentifier:englishUiAutomationName];
}

void SetUILabelScaledFont(UILabel* label, UIFont* font) {
  label.font = [[UIFontMetrics defaultMetrics] scaledFontForFont:font];
  label.adjustsFontForContentSizeCategory = YES;
}

void MaybeSetUILabelScaledFont(BOOL maybe, UILabel* label, UIFont* font) {
  if (maybe) {
    SetUILabelScaledFont(label, font);
  } else {
    label.font = font;
  }
}

void SetUITextFieldScaledFont(UITextField* textField, UIFont* font) {
  textField.font = [[UIFontMetrics defaultMetrics] scaledFontForFont:font];
  textField.adjustsFontForContentSizeCategory = YES;
}

void MaybeSetUITextFieldScaledFont(BOOL maybe,
                                   UITextField* textField,
                                   UIFont* font) {
  if (maybe) {
    SetUITextFieldScaledFont(textField, font);
  } else {
    textField.font = font;
  }
}

UIImage* CaptureViewWithOption(UIView* view,
                               CGFloat scale,
                               CaptureViewOption option) {
  UIGraphicsBeginImageContextWithOptions(view.bounds.size, NO /* not opaque */,
                                         scale);
  if (option != kClientSideRendering) {
    [view drawViewHierarchyInRect:view.bounds
               afterScreenUpdates:option == kAfterScreenUpdate];
  } else {
    CGContext* context = UIGraphicsGetCurrentContext();
    [view.layer renderInContext:context];
  }
  UIImage* image = UIGraphicsGetImageFromCurrentImageContext();
  UIGraphicsEndImageContext();
  return image;
}

UIImage* CaptureView(UIView* view, CGFloat scale) {
  return CaptureViewWithOption(view, scale, kNoCaptureOption);
}

UIImage* GreyImage(UIImage* image) {
  DCHECK(image);
  // Grey images are always non-retina to improve memory performance.
  UIGraphicsBeginImageContextWithOptions(image.size, YES, 1.0);
  CGRect greyImageRect = CGRectMake(0, 0, image.size.width, image.size.height);
  [image drawInRect:greyImageRect blendMode:kCGBlendModeLuminosity alpha:1.0];
  UIImage* greyImage = UIGraphicsGetImageFromCurrentImageContext();
  UIGraphicsEndImageContext();
  return greyImage;
}

UIImage* NativeReversableImage(int imageID, BOOL reversable) {
  DCHECK(imageID);
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  UIImage* image = rb.GetNativeImageNamed(imageID).ToUIImage();
  return (reversable && UseRTLLayout())
             ? [image imageFlippedForRightToLeftLayoutDirection]
             : image;
}

UIImage* NativeImage(int imageID) {
  return NativeReversableImage(imageID, NO);
}

UIImage* TintImage(UIImage* image, UIColor* color) {
  DCHECK(image);
  DCHECK(image.CGImage);
  DCHECK_GE(image.size.width * image.size.height, 1);
  DCHECK(color);

  CGRect rect = {CGPointZero, image.size};

  UIGraphicsBeginImageContextWithOptions(rect.size /* bitmap size */,
                                         NO /* opaque? */,
                                         0.0 /* main screen scale */);
  CGContextRef imageContext = UIGraphicsGetCurrentContext();
  CGContextSetShouldAntialias(imageContext, true);
  CGContextSetInterpolationQuality(imageContext, kCGInterpolationHigh);

  // CoreGraphics and UIKit uses different axis. UIKit's y points downards,
  // while CoreGraphic's points upwards. To keep the image correctly oriented,
  // apply a mirror around the X axis by inverting the Y coordinates.
  CGContextScaleCTM(imageContext, 1, -1);
  CGContextTranslateCTM(imageContext, 0, -rect.size.height);

  CGContextDrawImage(imageContext, rect, image.CGImage);
  CGContextSetBlendMode(imageContext, kCGBlendModeSourceIn);
  CGContextSetFillColorWithColor(imageContext, color.CGColor);
  CGContextFillRect(imageContext, rect);

  UIImage* outputImage = UIGraphicsGetImageFromCurrentImageContext();
  UIGraphicsEndImageContext();

  // Port the cap insets to the new image.
  if (!UIEdgeInsetsEqualToEdgeInsets(image.capInsets, UIEdgeInsetsZero)) {
    outputImage = [outputImage resizableImageWithCapInsets:image.capInsets];
  }

  // Port the flipping status to the new image.
  if (image.flipsForRightToLeftLayoutDirection) {
    outputImage = [outputImage imageFlippedForRightToLeftLayoutDirection];
  }

  return outputImage;
}

UIInterfaceOrientation GetInterfaceOrientation(UIWindow* window) {
  return window.windowScene.interfaceOrientation;
}

UIActivityIndicatorView* GetMediumUIActivityIndicatorView() {
  return [[UIActivityIndicatorView alloc]
      initWithActivityIndicatorStyle:UIActivityIndicatorViewStyleMedium];
}

UIActivityIndicatorView* GetLargeUIActivityIndicatorView() {
  return [[UIActivityIndicatorView alloc]
      initWithActivityIndicatorStyle:UIActivityIndicatorViewStyleLarge];
}

CGFloat CurrentKeyboardHeight(NSValue* keyboardFrameValue) {
  return [keyboardFrameValue CGRectValue].size.height;
}

UIImage* ImageWithColor(UIColor* color) {
  CGRect rect = CGRectMake(0, 0, 1, 1);
  UIGraphicsBeginImageContext(rect.size);
  CGContextRef context = UIGraphicsGetCurrentContext();
  CGContextSetFillColorWithColor(context, [color CGColor]);
  CGContextFillRect(context, rect);
  UIImage* image = UIGraphicsGetImageFromCurrentImageContext();
  UIGraphicsEndImageContext();
  return image;
}

UIImage* CircularImageFromImage(UIImage* image, CGFloat width) {
  CGRect frame =
      CGRectMakeAlignedAndCenteredAt(width / 2.0, width / 2.0, width);

  UIGraphicsBeginImageContextWithOptions(frame.size, NO, 0.0);
  CGContextRef context = UIGraphicsGetCurrentContext();

  CGContextBeginPath(context);
  CGContextAddEllipseInRect(context, frame);
  CGContextClosePath(context);
  CGContextClip(context);

  CGFloat scaleX = frame.size.width / image.size.width;
  CGFloat scaleY = frame.size.height / image.size.height;
  CGFloat scale = std::max(scaleX, scaleY);
  CGContextScaleCTM(context, scale, scale);

  [image drawInRect:CGRectMake(0, 0, image.size.width, image.size.height)];

  image = UIGraphicsGetImageFromCurrentImageContext();
  UIGraphicsEndImageContext();

  return image;
}

bool IsPortrait(UIWindow* window) {
  UIInterfaceOrientation orient = GetInterfaceOrientation(window);
  return UIInterfaceOrientationIsPortrait(orient) ||
         orient == UIInterfaceOrientationUnknown;
}

bool IsLandscape(UIWindow* window) {
  return UIInterfaceOrientationIsLandscape(GetInterfaceOrientation(window));
}

bool IsCompactWidth(id<UITraitEnvironment> environment) {
  return IsCompactWidth(environment.traitCollection);
}

bool IsCompactWidth(UITraitCollection* traitCollection) {
  return traitCollection.horizontalSizeClass == UIUserInterfaceSizeClassCompact;
}

bool IsCompactHeight(id<UITraitEnvironment> environment) {
  return IsCompactHeight(environment.traitCollection);
}

bool IsCompactHeight(UITraitCollection* traitCollection) {
  return traitCollection.verticalSizeClass == UIUserInterfaceSizeClassCompact;
}

bool IsRegularXRegularSizeClass(id<UITraitEnvironment> environment) {
  return IsRegularXRegularSizeClass(environment.traitCollection);
}

bool IsRegularXRegularSizeClass(UITraitCollection* traitCollection) {
  return traitCollection.verticalSizeClass == UIUserInterfaceSizeClassRegular &&
         traitCollection.horizontalSizeClass == UIUserInterfaceSizeClassRegular;
}

bool ShouldShowCompactToolbar(id<UITraitEnvironment> environment) {
  return ShouldShowCompactToolbar(environment.traitCollection);
}

bool ShouldShowCompactToolbar(UITraitCollection* traitCollection) {
  return !IsRegularXRegularSizeClass(traitCollection);
}

bool IsSplitToolbarMode(id<UITraitEnvironment> environment) {
  return IsSplitToolbarMode(environment.traitCollection);
}

bool IsSplitToolbarMode(UITraitCollection* traitCollection) {
  return IsCompactWidth(traitCollection) && !IsCompactHeight(traitCollection);
}

UIView* GetFirstResponderSubview(UIView* view) {
  if ([view isFirstResponder])
    return view;

  for (UIView* subview in [view subviews]) {
    UIView* firstResponder = GetFirstResponderSubview(subview);
    if (firstResponder)
      return firstResponder;
  }

  return nil;
}

UIResponder* GetFirstResponder() {
  DCHECK_CURRENTLY_ON(web::WebThread::UI);
  return GetFirstResponderSubview(GetAnyKeyWindow());
}

// Trigger a haptic vibration for the user selecting an action. This is a no-op
// for devices that do not support it.
void TriggerHapticFeedbackForImpact(UIImpactFeedbackStyle impactStyle) {
  // Although Apple documentation claims that UIFeedbackGenerator and its
  // concrete subclasses are available on iOS 10+, they are not really
  // available on an app whose deployment target is iOS 10.0 (iOS 10.1+ are
  // okay) and Chrome will fail at dynamic link time and instantly crash.
  // NSClassFromString() checks if Objective-C run-time has the class before
  // using it.
  Class generatorClass = NSClassFromString(@"UIImpactFeedbackGenerator");
  if (generatorClass) {
    UIImpactFeedbackGenerator* generator =
        [[generatorClass alloc] initWithStyle:impactStyle];
    [generator impactOccurred];
  }
}

// Trigger a haptic vibration for the change in selection. This is a no-op for
// devices that do not support it.
void TriggerHapticFeedbackForSelectionChange() {
  // Although Apple documentation claims that UIFeedbackGenerator and its
  // concrete subclasses are available on iOS 10+, they are not really
  // available on an app whose deployment target is iOS 10.0 (iOS 10.1+ are
  // okay) and Chrome will fail at dynamic link time and instantly crash.
  // NSClassFromString() checks if Objective-C run-time has the class before
  // using it.
  Class generatorClass = NSClassFromString(@"UISelectionFeedbackGenerator");
  if (generatorClass) {
    UISelectionFeedbackGenerator* generator = [[generatorClass alloc] init];
    [generator selectionChanged];
  }
}

// Trigger a haptic vibration for a notification. This is a no-op for devices
// that do not support it.
void TriggerHapticFeedbackForNotification(UINotificationFeedbackType type) {
  // Although Apple documentation claims that UIFeedbackGenerator and its
  // concrete subclasses are available on iOS 10+, they are not really
  // available on an app whose deployment target is iOS 10.0 (iOS 10.1+ are
  // okay) and Chrome will fail at dynamic link time and instantly crash.
  // NSClassFromString() checks if Objective-C run-time has the class before
  // using it.
  Class generatorClass = NSClassFromString(@"UINotificationFeedbackGenerator");
  if (generatorClass) {
    UINotificationFeedbackGenerator* generator = [[generatorClass alloc] init];
    [generator notificationOccurred:type];
  }
}

NSString* TextForTabCount(long count) {
  if (count <= 0)
    return @"";
  if (count > 99)
    return @":)";
  return [NSString stringWithFormat:@"%ld", count];
}

void RegisterEditMenuItem(UIMenuItem* item) {
  UIMenuController* menu = [UIMenuController sharedMenuController];
  NSArray<UIMenuItem*>* items = [menu menuItems];

  for (UIMenuItem* existingItem in items) {
    if ([existingItem action] == [item action]) {
      return;
    }
  }

  items = items ? [items arrayByAddingObject:item] : @[ item ];

  [menu setMenuItems:items];
}

UIView* ViewHierarchyRootForView(UIView* view) {
  if (view.window)
    return view.window;

  if (!view.superview)
    return view;

  return ViewHierarchyRootForView(view.superview);
}
