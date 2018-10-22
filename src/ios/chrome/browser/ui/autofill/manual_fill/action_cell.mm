// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/autofill/manual_fill/action_cell.h"

#import "ios/chrome/browser/ui/autofill/manual_fill/uicolor_manualfill.h"
#import "ios/chrome/common/ui_util/constraints_ui_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Left and right margins of the cell contents.
static const CGFloat sideMargins = 16;
// The base multiplier for the top and bottom margins.  This number multiplied
// by the font size plus the base margins will give similar results to
// |constraintEqualToSystemSpacingBelowAnchor:|.
static const CGFloat iOS10MarginFontMultiplier = 1.18;
// The base top margin, only used in iOS 10. Refer to
// |iOS10MarginFontMultiplier| for how it is used.
static const CGFloat iOS10BaseTopMargin = 24;
// The base bottom margin, only used in iOS 10. Refer to
// |iOS10MarginFontMultiplier| for how it is used.
static const CGFloat iOS10BaseBottomMargin = 6;
// The multiplier for the base system spacing at the top margin.
static const CGFloat TopBaseSystemSpacingMultiplier = 1.78;
// The multiplier for the base system spacing at the bottom margin.
static const CGFloat BottomBaseSystemSpacingMultiplier = 2.26;
}  // namespace

@interface ManualFillActionCell ()
// The action block to be called when the user taps the title button.
@property(nonatomic, copy) void (^action)(void);
// The title button of this cell.
@property(nonatomic, strong) UIButton* titleButton;
@end

@implementation ManualFillActionCell
@synthesize action = _action;
@synthesize titleButton = _titleButton;

#pragma mark - Public

- (void)setUpWithTitle:(NSString*)title action:(void (^)(void))action {
  if (self.contentView.subviews.count == 0) {
    [self createView];
  }
  [self.titleButton setTitle:title forState:UIControlStateNormal];
  self.action = action;
}

- (void)prepareForReuse {
  [super prepareForReuse];
  self.action = nil;
}

#pragma mark - Private

- (void)createView {
  self.selectionStyle = UITableViewCellSelectionStyleNone;

  self.titleButton = [[UIButton alloc] init];
  [self.titleButton setTitleColor:UIColor.cr_manualFillTintColor
                         forState:UIControlStateNormal];
  self.titleButton.translatesAutoresizingMaskIntoConstraints = NO;
  self.titleButton.titleLabel.font =
      [UIFont preferredFontForTextStyle:UIFontTextStyleBody];
  self.titleButton.titleLabel.adjustsFontForContentSizeCategory = YES;
  [self.titleButton addTarget:self
                       action:@selector(userDidTapTitleButton:)
             forControlEvents:UIControlEventTouchUpInside];
  self.titleButton.titleLabel.lineBreakMode = NSLineBreakByTruncatingTail;
  [self.contentView addSubview:self.titleButton];
  id<LayoutGuideProvider> safeArea =
      SafeAreaLayoutGuideForView(self.contentView);

  NSArray* verticalConstraints;
  if (@available(iOS 11, *)) {
    // Multipliers of these constraints are calculated based on a 24 base
    // system spacing.
    verticalConstraints = @[
      // Vertical constraints.
      [self.titleButton.firstBaselineAnchor
          constraintEqualToSystemSpacingBelowAnchor:self.contentView.topAnchor
                                         multiplier:
                                             TopBaseSystemSpacingMultiplier],
      [self.contentView.bottomAnchor
          constraintEqualToSystemSpacingBelowAnchor:self.titleButton
                                                        .lastBaselineAnchor
                                         multiplier:
                                             BottomBaseSystemSpacingMultiplier],
    ];
  } else {
    CGFloat pointSize = self.titleButton.titleLabel.font.pointSize;
    // These margins are based on the design size and the current point size.
    // The multipliers were selected by manually testing the different system
    // font sizes.
    CGFloat marginTop =
        iOS10BaseTopMargin + pointSize * iOS10MarginFontMultiplier;
    CGFloat marginBottom =
        iOS10BaseBottomMargin + pointSize * iOS10MarginFontMultiplier;

    verticalConstraints = @[
      [self.titleButton.firstBaselineAnchor
          constraintEqualToAnchor:self.contentView.topAnchor
                         constant:marginTop],
      [self.contentView.bottomAnchor
          constraintEqualToAnchor:self.titleButton.lastBaselineAnchor
                         constant:marginBottom],
    ];
  }
  [NSLayoutConstraint activateConstraints:verticalConstraints];
  // Horizontal constraints.
  [NSLayoutConstraint activateConstraints:@[
    [self.titleButton.leadingAnchor
        constraintEqualToAnchor:safeArea.leadingAnchor
                       constant:sideMargins],
    [safeArea.trailingAnchor
        constraintGreaterThanOrEqualToAnchor:self.titleButton.trailingAnchor
                                    constant:sideMargins],
  ]];
}

- (void)userDidTapTitleButton:(UIButton*)sender {
  if (self.action) {
    self.action();
  }
}

@end
