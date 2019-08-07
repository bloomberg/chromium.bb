// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <EarlGrey/EarlGrey.h>
#import <GTXiLib/GTXiLib.h>
#import <XCTest/XCTest.h>

#include "ios/chrome/test/earl_grey/accessibility_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Returns whether a UIView is hidden from the screen, based on the alpha
// and hidden properties of the UIView.
bool ViewIsHidden(UIView* view) {
  return view.alpha == 0 || view.hidden;
}

// Returns whether a view should be accessible. A view should be accessible if
// either it is a whitelisted type or its isAccessibilityElement property is
// enabled.
bool ViewShouldBeAccessible(UIView* view) {
  NSArray* classList = @[
    [UILabel class], [UIControl class], [UITableView class],
    [UICollectionViewCell class]
  ];
  bool viewIsAccessibleType = false;
  for (Class accessibleClass in classList) {
    if ([view isKindOfClass:accessibleClass]) {
      viewIsAccessibleType = true;
      break;
    }
  }
  return (viewIsAccessibleType || view.isAccessibilityElement) &&
         !ViewIsHidden(view);
}

// Returns true if |element| or descendant has property accessibilityLabel
// that is not an empty string. If |element| has isAccessibilityElement set to
// true, then the |element| itself must have a label.
bool ViewOrDescendantHasAccessibilityLabel(UIView* element) {
  if ([element.accessibilityLabel length])
    return true;
  if (element.isAccessibilityElement)
    return false;
  for (UIView* view in element.subviews) {
    if (ViewOrDescendantHasAccessibilityLabel(view))
      return true;
  }
  return false;
}

// Returns an array of elements that should be accessible.
// Helper method for accessibilityElementsStartingFromView, so that
// |ancestorString|, which handles internal bookkeeping, is hidden from the top
// level API. |ancestorString| is the description of the most recent ancestor
// with isAccessibilityElement set to true, and thus when the method is first
// called, it should always be set to nil.
NSArray* AccessibilityElementsHelperStartingFromView(UIView* view,
                                                     NSString* ancestorString,
                                                     NSError** error) {
  NSMutableArray* results = [NSMutableArray array];
  // Add |view| to |results| if it should be accessible and is not hidden.
  if (ViewShouldBeAccessible(view)) {
    // UILabels have an extra check since some labels are dynamically set, and
    // may not have text or an accessibilityLabel at a given point of
    // execution.
    if ([view isKindOfClass:[UILabel class]]) {
      UILabel* label = static_cast<UILabel*>(view);
      if ([label.text length])
        [results addObject:label];
    } else {
      [results addObject:view];
      if ([ancestorString length]) {
        if (error != nil && !*error)
          *error = [NSError errorWithDomain:@"Ancestor blocks VoiceOver"
                                       code:1
                                   userInfo:nil];
        // The most recent ancestor with Accessibility Element set to true
        // blocks VoiceOver for Element ancestorString
      }
    }
  }
  if (view.isAccessibilityElement && ![view isKindOfClass:[UILabel class]]) {
    ancestorString = [view description];
  }
  if (![view isKindOfClass:[UITableView class]]) {
    // Do not recurse below views which are accessible but may have children
    // that default to being accessible. Also, do not recurse below views which
    // implement the UIAccessibilityContainer informal protocol, as these views
    // have taken ownership of accessibility behavior of descendents.
    if ([view isKindOfClass:[UISwitch class]] ||
        [view respondsToSelector:@selector(accessibilityElements)])
      return results;
    for (UIView* subView in [view subviews]) {
      [results addObjectsFromArray:AccessibilityElementsHelperStartingFromView(
                                       subView, ancestorString, error)];
    }
  }
  return results;
}
}

namespace chrome_test_util {

void VerifyAccessibilityForCurrentScreen() {
  GTXToolKit* toolkit = [[GTXToolKit alloc] init];
  NSError* error = nil;
  for (UIWindow* window in [[UIApplication sharedApplication] windows]) {
    // Run the checks on all elements on the screen.
    BOOL success =
        [toolkit checkAllElementsFromRootElements:@[ window ] error:&error];
    GREYAssert(success, @"Accessibility checks failed! Error: %@", error);
  }
}

}  // namespace chrome_test_util
