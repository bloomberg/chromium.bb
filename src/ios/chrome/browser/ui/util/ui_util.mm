// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/util/ui_util.h"

#import <UIKit/UIKit.h>
#include <limits>

#include "base/feature_list.h"
#include "base/ios/ios_util.h"
#include "ios/chrome/app/tests_hook.h"
#import "ios/chrome/browser/ui/ui_feature_flags.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#include "ui/gfx/ios/uikit_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// The em-width value used to differentiate small and large devices.
// With Larger Text Off, Bold Text Off and the device orientation in portrait:
// iPhone 5s is considered as a small device, unlike iPhone 8 or iPhone 12 mini.
const CGFloat kSmallDeviceThreshold = 22.0;

}  // namespace

CGFloat CurrentScreenHeight() {
  return [UIScreen mainScreen].bounds.size.height;
}

CGFloat CurrentScreenWidth() {
  return [UIScreen mainScreen].bounds.size.width;
}

bool IsSmallDevice() {
  CGSize mSize = [@"m" sizeWithAttributes:@{
    NSFontAttributeName : [UIFont preferredFontForTextStyle:UIFontTextStyleBody]
  }];
  CGFloat emWidth = CurrentScreenWidth() / mSize.width;
  return emWidth < kSmallDeviceThreshold;
}

CGFloat DeviceCornerRadius() {
  UIUserInterfaceIdiom idiom = [[UIDevice currentDevice] userInterfaceIdiom];
  UIWindow* window = UIApplication.sharedApplication.windows.firstObject;
  const BOOL isRoundedDevice =
      (idiom == UIUserInterfaceIdiomPhone && window.safeAreaInsets.bottom);
  return isRoundedDevice ? 40.0 : 0.0;
}

CGFloat AlignValueToPixel(CGFloat value) {
  static CGFloat scale = [[UIScreen mainScreen] scale];
  return floor(value * scale) / scale;
}

CGPoint AlignPointToPixel(CGPoint point) {
  return CGPointMake(AlignValueToPixel(point.x), AlignValueToPixel(point.y));
}

CGRect AlignRectToPixel(CGRect rect) {
  rect.origin = AlignPointToPixel(rect.origin);
  return rect;
}

CGRect AlignRectOriginAndSizeToPixels(CGRect rect) {
  rect.origin = AlignPointToPixel(rect.origin);
  rect.size = ui::AlignSizeToUpperPixel(rect.size);
  return rect;
}

CGRect CGRectMakeAlignedAndCenteredAt(CGFloat x, CGFloat y, CGFloat width) {
  return AlignRectOriginAndSizeToPixels(
      CGRectMake(x - width / 2.0, y - width / 2.0, width, width));
}

CGRect CGRectMakeCenteredRectInFrame(CGSize frameSize, CGSize rectSize) {
  CGFloat rectX = AlignValueToPixel((frameSize.width - rectSize.width) / 2);
  CGFloat rectY = AlignValueToPixel((frameSize.height - rectSize.height) / 2);
  return CGRectMake(rectX, rectY, rectSize.width, rectSize.height);
}

bool AreCGFloatsEqual(CGFloat a, CGFloat b) {
  return std::fabs(a - b) <= std::numeric_limits<CGFloat>::epsilon();
}

// Based on an original size and a target size applies the transformations.
void CalculateProjection(CGSize originalSize,
                         CGSize desiredTargetSize,
                         ProjectionMode projectionMode,
                         CGSize& targetSize,
                         CGRect& projectTo) {
  targetSize = desiredTargetSize;
  projectTo = CGRectZero;
  if (originalSize.height < 1 || originalSize.width < 1)
    return;
  if (targetSize.height < 1 || targetSize.width < 1)
    return;

  CGFloat aspectRatio = originalSize.width / originalSize.height;
  CGFloat targetAspectRatio = targetSize.width / targetSize.height;
  switch (projectionMode) {
    case ProjectionMode::kFill:
      // Don't preserve the aspect ratio.
      projectTo.size = targetSize;
      break;

    case ProjectionMode::kAspectFill:
    case ProjectionMode::kAspectFillAlignTop:
      if (targetAspectRatio < aspectRatio) {
        // Clip the x-axis.
        projectTo.size.width = targetSize.height * aspectRatio;
        projectTo.size.height = targetSize.height;
        projectTo.origin.x = (targetSize.width - projectTo.size.width) / 2;
        projectTo.origin.y = 0;
      } else {
        // Clip the y-axis.
        projectTo.size.width = targetSize.width;
        projectTo.size.height = targetSize.width / aspectRatio;
        projectTo.origin.x = 0;
        projectTo.origin.y = (targetSize.height - projectTo.size.height) / 2;
      }
      if (projectionMode == ProjectionMode::kAspectFillAlignTop) {
        projectTo.origin.y = 0;
      }
      break;

    case ProjectionMode::kAspectFit:
      if (targetAspectRatio < aspectRatio) {
        projectTo.size.width = targetSize.width;
        projectTo.size.height = projectTo.size.width / aspectRatio;
        targetSize = projectTo.size;
      } else {
        projectTo.size.height = targetSize.height;
        projectTo.size.width = projectTo.size.height * aspectRatio;
        targetSize = projectTo.size;
      }
      break;

    case ProjectionMode::kAspectFillNoClipping:
      if (targetAspectRatio < aspectRatio) {
        targetSize.width = targetSize.height * aspectRatio;
        targetSize.height = targetSize.height;
      } else {
        targetSize.width = targetSize.width;
        targetSize.height = targetSize.width / aspectRatio;
      }
      projectTo.size = targetSize;
      break;
  }

  projectTo = CGRectIntegral(projectTo);
  // There's no CGSizeIntegral, faking one instead.
  CGRect integralRect = CGRectZero;
  integralRect.size = targetSize;
  targetSize = CGRectIntegral(integralRect).size;
}
