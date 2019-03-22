// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/autofill/manual_fill/manual_fill_cell_utils.h"

#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#import "ios/chrome/browser/ui/autofill/manual_fill/manual_fill_cell_button.h"
#import "ios/chrome/browser/ui/autofill/manual_fill/uicolor_manualfill.h"
#import "ios/chrome/common/ui_util/constraints_ui_util.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

UIButton* CreateButtonWithSelectorAndTarget(SEL action, id target) {
  UIButton* button = [ManualFillCellButton buttonWithType:UIButtonTypeCustom];
  [button setTitleColor:UIColor.cr_manualFillTintColor
               forState:UIControlStateNormal];
  button.translatesAutoresizingMaskIntoConstraints = NO;
  button.titleLabel.font =
      [UIFont preferredFontForTextStyle:UIFontTextStyleBody];
  button.titleLabel.adjustsFontForContentSizeCategory = YES;
  button.contentHorizontalAlignment =
      UIControlContentHorizontalAlignmentLeading;
  [button addTarget:target
                action:action
      forControlEvents:UIControlEventTouchUpInside];
  button.contentEdgeInsets =
      UIEdgeInsetsMake(ButtonVerticalMargin, ButtonHorizontalMargin,
                       ButtonVerticalMargin, ButtonHorizontalMargin);
  return button;
}

void AppendVerticalConstraintsSpacingForViews(
    NSMutableArray<NSLayoutConstraint*>* constraints,
    NSArray<UIView*>* views,
    UIView* container) {
  AppendVerticalConstraintsSpacingForViews(
      constraints, views, container, TopSystemSpacingMultiplier,
      MiddleSystemSpacingMultiplier, BottomSystemSpacingMultiplier);
}

void AppendVerticalConstraintsSpacingForViews(
    NSMutableArray<NSLayoutConstraint*>* constraints,
    NSArray<UIView*>* views,
    UIView* container,
    CGFloat topSystemSpacingMultiplier,
    CGFloat middleSystemSpacingMultiplier,
    CGFloat bottomSystemSpacingMultiplier) {
  // Multipliers of these constraints are calculated based on a 24 base
  // system spacing.
  NSLayoutYAxisAnchor* previousAnchor = container.topAnchor;
  CGFloat multiplier = topSystemSpacingMultiplier;
  for (UIView* view in views) {
    [constraints
        addObject:[view.firstBaselineAnchor
                      constraintEqualToSystemSpacingBelowAnchor:previousAnchor
                                                     multiplier:multiplier]];
    multiplier = middleSystemSpacingMultiplier;
    previousAnchor = view.lastBaselineAnchor;
  }
  multiplier = bottomSystemSpacingMultiplier;
  [constraints
      addObject:[container.bottomAnchor
                    constraintEqualToSystemSpacingBelowAnchor:previousAnchor
                                                   multiplier:multiplier]];
}

void AppendHorizontalConstraintsForViews(
    NSMutableArray<NSLayoutConstraint*>* constraints,
    NSArray<UIView*>* views,
    UIView* guide) {
  AppendHorizontalConstraintsForViews(constraints, views, guide, 0);
}

void AppendHorizontalConstraintsForViews(
    NSMutableArray<NSLayoutConstraint*>* constraints,
    NSArray<UIView*>* views,
    UIView* guide,
    CGFloat margin) {
  AppendHorizontalConstraintsForViews(constraints, views, guide, margin, 0);
}

void AppendHorizontalConstraintsForViews(
    NSMutableArray<NSLayoutConstraint*>* constraints,
    NSArray<UIView*>* views,
    UIView* guide,
    CGFloat margin,
    AppendConstraints options) {
  if (views.count == 0)
    return;

  NSLayoutXAxisAnchor* previousAnchor = guide.leadingAnchor;
  UILayoutPriority firstPriority =
      options & AppendConstraintsHorizontalExtraSpaceLeft
          ? UILayoutPriorityDefaultHigh
          : UILayoutPriorityDefaultLow;
  UILayoutPriority lastPriority =
      options & AppendConstraintsHorizontalExtraSpaceLeft
          ? UILayoutPriorityDefaultLow
          : UILayoutPriorityDefaultHigh;

  CGFloat shift = margin;
  for (UIView* view in views) {
    [constraints
        addObject:[view.leadingAnchor constraintEqualToAnchor:previousAnchor
                                                     constant:shift]];
    [view setContentCompressionResistancePriority:firstPriority
                                          forAxis:
                                              UILayoutConstraintAxisHorizontal];
    [view setContentHuggingPriority:lastPriority
                            forAxis:UILayoutConstraintAxisHorizontal];
    previousAnchor = view.trailingAnchor;
    shift = 0;
  }

  [constraints addObject:[views.lastObject.trailingAnchor
                             constraintEqualToAnchor:guide.trailingAnchor
                                            constant:-margin]];
  // Give all remaining space to the last button, minus margin, as per UX.
  [views.lastObject
      setContentCompressionResistancePriority:lastPriority
                                      forAxis:UILayoutConstraintAxisHorizontal];
  [views.lastObject setContentHuggingPriority:firstPriority
                                      forAxis:UILayoutConstraintAxisHorizontal];

  if (options & AppendConstraintsHorizontalSyncBaselines) {
    AppendEqualBaselinesConstraints(constraints, views);
  }
}

void AppendEqualBaselinesConstraints(
    NSMutableArray<NSLayoutConstraint*>* constraints,
    NSArray<UIView*>* views) {
  UIView* leadingView = views.firstObject;
  for (UIView* view in views) {
    DCHECK([view isKindOfClass:[UIButton class]] ||
           [view isKindOfClass:[UILabel class]]);
    if (view == leadingView)
      continue;
    [constraints
        addObject:[view.lastBaselineAnchor
                      constraintEqualToAnchor:leadingView.lastBaselineAnchor]];
  }
}

UILabel* CreateLabel() {
  UILabel* label = [[UILabel alloc] init];
  label.translatesAutoresizingMaskIntoConstraints = NO;
  label.adjustsFontForContentSizeCategory = YES;
  label.font = [UIFont preferredFontForTextStyle:UIFontTextStyleBody];
  return label;
}

UIView* CreateGraySeparatorForContainer(UIView* container) {
  UIView* grayLine = [[UIView alloc] init];
  grayLine.backgroundColor = UIColor.cr_manualFillGrayLineColor;
  grayLine.translatesAutoresizingMaskIntoConstraints = NO;
  [container addSubview:grayLine];

  id<LayoutGuideProvider> safeArea = container.safeAreaLayoutGuide;
  [NSLayoutConstraint activateConstraints:@[
    // Vertical constraints.
    [grayLine.bottomAnchor constraintEqualToAnchor:container.bottomAnchor],
    [grayLine.heightAnchor constraintEqualToConstant:1],
    // Horizontal constraints.
    [grayLine.leadingAnchor constraintEqualToAnchor:safeArea.leadingAnchor
                                           constant:ButtonHorizontalMargin],
    [safeArea.trailingAnchor constraintEqualToAnchor:grayLine.trailingAnchor
                                            constant:ButtonHorizontalMargin],
  ]];

  return grayLine;
}
