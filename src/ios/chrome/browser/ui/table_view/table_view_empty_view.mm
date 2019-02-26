// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/table_view/table_view_empty_view.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// The StackView vertical spacing.
const float kStackViewVerticalSpacing = 23.0;
// The StackView width.
const float kStackViewWidth = 227.0;
// Returns |message| as an attributed string with default styling.
NSAttributedString* GetAttributedMessage(NSString* message) {
  NSMutableParagraphStyle* paragraph_style =
      [[NSMutableParagraphStyle alloc] init];
  paragraph_style.lineBreakMode = NSLineBreakByWordWrapping;
  paragraph_style.alignment = NSTextAlignmentCenter;
  NSDictionary* default_attributes = @{
    NSFontAttributeName :
        [UIFont preferredFontForTextStyle:UIFontTextStyleBody],
    NSForegroundColorAttributeName : [UIColor grayColor],
    NSParagraphStyleAttributeName : paragraph_style
  };
  return [[NSAttributedString alloc] initWithString:message
                                         attributes:default_attributes];
}
}

@interface TableViewEmptyView ()
// The message that will be displayed and the label that will display it.
@property(nonatomic, copy) NSAttributedString* message;
@property(nonatomic, strong) UILabel* messageLabel;
// The image that will be displayed.
@property(nonatomic, strong) UIImage* image;
@end

@implementation TableViewEmptyView
@synthesize message = _message;
@synthesize messageLabel = _messageLabel;
@synthesize image = _image;

- (instancetype)initWithFrame:(CGRect)frame
                      message:(NSString*)message
                        image:(UIImage*)image {
  if (self = [super initWithFrame:frame]) {
    _message = GetAttributedMessage(message);
    _image = image;
    self.accessibilityIdentifier = [[self class] accessibilityIdentifier];
  }
  return self;
}

- (instancetype)initWithFrame:(CGRect)frame
            attributedMessage:(NSAttributedString*)message
                        image:(UIImage*)image {
  if (self = [super initWithFrame:frame]) {
    _message = message;
    _image = image;
    self.accessibilityIdentifier = [[self class] accessibilityIdentifier];
  }
  return self;
}

#pragma mark - Accessors

- (NSString*)messageAccessibilityLabel {
  return self.messageLabel.accessibilityLabel;
}

- (void)setMessageAccessibilityLabel:(NSString*)label {
  if ([self.messageAccessibilityLabel isEqualToString:label])
    return;
  self.messageLabel.accessibilityLabel = label;
}

#pragma mark - Public

+ (NSString*)accessibilityIdentifier {
  return @"TableViewEmptyView";
}

#pragma mark - UIView

- (void)willMoveToSuperview:(UIView*)newSuperview {
  // Return if the subviews have already been created and added.
  if (!(self.subviews.count == 0))
    return;

  UIImageView* imageView = [[UIImageView alloc] initWithImage:self.image];
  imageView.contentMode = UIViewContentModeScaleAspectFit;
  imageView.clipsToBounds = YES;

  UILabel* messageLabel = [[UILabel alloc] init];
  messageLabel.numberOfLines = 0;
  messageLabel.attributedText = self.message;
  messageLabel.accessibilityLabel = self.message.string;
  self.messageLabel = messageLabel;

  // Vertical stack view that holds the image and message.
  UIStackView* verticalStack = [[UIStackView alloc]
      initWithArrangedSubviews:@[ imageView, messageLabel ]];
  verticalStack.axis = UILayoutConstraintAxisVertical;
  verticalStack.spacing = kStackViewVerticalSpacing;
  verticalStack.distribution = UIStackViewDistributionFill;
  verticalStack.translatesAutoresizingMaskIntoConstraints = NO;
  [self addSubview:verticalStack];

  [NSLayoutConstraint activateConstraints:@[
    [verticalStack.centerYAnchor constraintEqualToAnchor:self.centerYAnchor],
    [verticalStack.centerXAnchor constraintEqualToAnchor:self.centerXAnchor],
    [verticalStack.widthAnchor constraintEqualToConstant:kStackViewWidth]
  ]];
}

@end
