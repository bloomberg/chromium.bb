// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/alert_view_controller/alert_view_controller.h"

#include "base/logging.h"
#import "ios/chrome/browser/ui/alert_view_controller/alert_action.h"
#import "ios/chrome/browser/ui/elements/gray_highlight_button.h"
#import "ios/chrome/browser/ui/elements/text_field_configuration.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/colors/UIColor+cr_semantic_colors.h"
#import "ios/chrome/common/colors/semantic_color_names.h"
#import "ios/chrome/common/ui_util/constraints_ui_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Properties of the alert shadow.
constexpr CGFloat kShadowOffsetX = 0;
constexpr CGFloat kShadowOffsetY = 15;
constexpr CGFloat kShadowRadius = 13;
constexpr float kShadowOpacity = 0.12;

// Properties of the alert view.
constexpr CGFloat kCornerRadius = 14;
constexpr CGFloat kAlertWidth = 270;
constexpr CGFloat kAlertWidthAccessibilty = 402;
constexpr CGFloat kTextFieldCornerRadius = 5;
constexpr CGFloat kMinimumHeight = 30;
constexpr CGFloat kMinimumMargin = 4;

// Inset at the top of the alert. Is always present.
constexpr CGFloat kAlertMarginTop = 22;
// Space before the actions and everything else.
constexpr CGFloat kAlertActionsSpacing = 12;

// Insets for the content in the alert view.
constexpr CGFloat kTitleInsetLeading = 20;
constexpr CGFloat kTitleInsetBottom = 9;
constexpr CGFloat kTitleInsetTrailing = 20;

constexpr CGFloat kMessageInsetLeading = 20;
constexpr CGFloat kMessageInsetBottom = 6;
constexpr CGFloat kMessageInsetTrailing = 20;

constexpr CGFloat kButtonInsetTop = 13;
constexpr CGFloat kButtonInsetLeading = 20;
constexpr CGFloat kButtonInsetBottom = 13;
constexpr CGFloat kButtonInsetTrailing = 20;

constexpr CGFloat kTextfieldStackInsetTop = 12;
constexpr CGFloat kTextfieldStackInsetLeading = 12;
constexpr CGFloat kTextfieldStackInsetTrailing = 12;

constexpr CGFloat kTextfieldInset = 8;

// This is how many bits UIViewAnimationCurve needs to be shifted to be in
// UIViewAnimationOptions format. Must match the one in UIView.h.
constexpr NSUInteger kUIViewAnimationCurveToOptionsShift = 16;

}  // namespace

@interface AlertViewController () <UITextFieldDelegate,
                                   UIGestureRecognizerDelegate>

// The actions for to this alert. |copy| for safety against mutable objects.
@property(nonatomic, copy) NSArray<AlertAction*>* actions;

// This maps UIButtons' tags with AlertActions' uniqueIdentifiers.
@property(nonatomic, strong)
    NSMutableDictionary<NSNumber*, AlertAction*>* buttonAlertActionsDictionary;

// This is the view with the shadow, white background and round corners.
// Everything will be added here.
@property(nonatomic, strong) UIView* contentView;

// The message of the alert, will appear after the title.
@property(nonatomic, copy) NSString* message;

// Text field configurations for this alert. One text field will be created for
// each |TextFieldConfiguration|. |copy| for safety against mutable objects.
@property(nonatomic, copy)
    NSArray<TextFieldConfiguration*>* textFieldConfigurations;

// The text fields that had been added to this alert.
@property(nonatomic, strong) NSArray<UITextField*>* textFields;

// Recognizer used to dismiss the keyboard when tapping outside the container
// view.
@property(nonatomic, strong) UITapGestureRecognizer* tapRecognizer;

// Recognizer used to dismiss the keyboard swipping down the alert.
@property(nonatomic, strong) UISwipeGestureRecognizer* swipeRecognizer;

// This is the last focused text field, the gestures to dismiss the keyboard
// will end up calling |resignFirstResponder| on this.
@property(nonatomic, weak) UITextField* lastFocusedTextField;

// This holds the text field stack view. A reference is needed because its
// layer.borderColor is manually set. As that is a CGColor, it must be updated
// when the trait collection changes from light to dark mode.
@property(nonatomic, weak) UIView* textFieldStackHolder;

@end

@implementation AlertViewController

#pragma mark - Public

- (void)traitCollectionDidChange:(UITraitCollection*)previousTraitCollection {
  [super traitCollectionDidChange:previousTraitCollection];

#if defined(__IPHONE_13_0) && (__IPHONE_OS_VERSION_MAX_ALLOWED >= __IPHONE_13_0)
  if (@available(iOS 13, *)) {
    if ([self.traitCollection
            hasDifferentColorAppearanceComparedToTraitCollection:
                previousTraitCollection]) {
      [self.traitCollection performAsCurrentTraitCollection:^{
        self.textFieldStackHolder.layer.borderColor =
            UIColor.cr_separatorColor.CGColor;
      }];
    }
  }
#endif
}

- (void)loadView {
  [super loadView];
  self.view.backgroundColor = [UIColor colorNamed:kScrimBackgroundColor];
  self.view.accessibilityViewIsModal = YES;

  self.tapRecognizer = [[UITapGestureRecognizer alloc]
      initWithTarget:self
              action:@selector(dismissKeyboard)];
  self.tapRecognizer.enabled = NO;
  self.tapRecognizer.delegate = self;
  [self.view addGestureRecognizer:self.tapRecognizer];

  self.contentView = [[UIView alloc] init];
  self.contentView.clipsToBounds = YES;
  self.contentView.backgroundColor = UIColor.cr_systemBackgroundColor;
  self.contentView.layer.cornerRadius = kCornerRadius;
  self.contentView.layer.shadowOffset =
      CGSizeMake(kShadowOffsetX, kShadowOffsetY);
  self.contentView.layer.shadowRadius = kShadowRadius;
  self.contentView.layer.shadowOpacity = kShadowOpacity;
  self.contentView.translatesAutoresizingMaskIntoConstraints = NO;
  [self.view addSubview:self.contentView];

  self.swipeRecognizer = [[UISwipeGestureRecognizer alloc]
      initWithTarget:self
              action:@selector(dismissKeyboard)];
  self.swipeRecognizer.direction = UISwipeGestureRecognizerDirectionDown;
  self.swipeRecognizer.enabled = NO;
  [self.contentView addGestureRecognizer:self.swipeRecognizer];

  auto GetAlertWidth = ^CGFloat(void) {
    BOOL isAccessibilityContentSize =
        UIContentSizeCategoryIsAccessibilityCategory(
            [UIApplication sharedApplication].preferredContentSizeCategory);
    return isAccessibilityContentSize ? kAlertWidthAccessibilty : kAlertWidth;
  };

  NSLayoutConstraint* widthConstraint =
      [self.contentView.widthAnchor constraintEqualToConstant:GetAlertWidth()];
  widthConstraint.priority = UILayoutPriorityRequired - 1;

  [[NSNotificationCenter defaultCenter]
      addObserverForName:UIContentSizeCategoryDidChangeNotification
                  object:nil
                   queue:[NSOperationQueue mainQueue]
              usingBlock:^(NSNotification* _Nonnull note) {
                widthConstraint.constant = GetAlertWidth();
              }];
  [NSLayoutConstraint activateConstraints:@[
    widthConstraint,

    // Centering
    [self.contentView.centerXAnchor
        constraintEqualToAnchor:self.view.safeAreaLayoutGuide.centerXAnchor],
    [self.contentView.centerYAnchor
        constraintEqualToAnchor:self.view.safeAreaLayoutGuide.centerYAnchor],

    // Minimum Size
    [self.contentView.heightAnchor
        constraintGreaterThanOrEqualToConstant:kMinimumHeight],

    // Maximum Size
    [self.contentView.topAnchor
        constraintGreaterThanOrEqualToAnchor:self.view.safeAreaLayoutGuide
                                                 .topAnchor
                                    constant:kMinimumMargin],
    [self.contentView.bottomAnchor
        constraintLessThanOrEqualToAnchor:self.view.safeAreaLayoutGuide
                                              .bottomAnchor
                                 constant:-kMinimumMargin],
    [self.contentView.trailingAnchor
        constraintLessThanOrEqualToAnchor:self.view.safeAreaLayoutGuide
                                              .trailingAnchor
                                 constant:-kMinimumMargin],
    [self.contentView.leadingAnchor
        constraintGreaterThanOrEqualToAnchor:self.view.safeAreaLayoutGuide
                                                 .leadingAnchor
                                    constant:kMinimumMargin],

  ]];

  UIScrollView* scrollView = [[UIScrollView alloc] init];
  scrollView.showsVerticalScrollIndicator = YES;
  scrollView.showsHorizontalScrollIndicator = NO;
  scrollView.translatesAutoresizingMaskIntoConstraints = NO;
  scrollView.scrollEnabled = YES;
  scrollView.contentInsetAdjustmentBehavior =
      UIScrollViewContentInsetAdjustmentAlways;
  [self.contentView addSubview:scrollView];
  AddSameConstraints(scrollView, self.contentView);

  UIStackView* stackView = [[UIStackView alloc] init];
  stackView.axis = UILayoutConstraintAxisVertical;
  stackView.translatesAutoresizingMaskIntoConstraints = NO;
  stackView.alignment = UIStackViewAlignmentCenter;
  [scrollView addSubview:stackView];

  NSLayoutConstraint* heightConstraint = [scrollView.heightAnchor
      constraintEqualToAnchor:scrollView.contentLayoutGuide.heightAnchor
                   multiplier:1];
  // UILayoutPriorityDefaultHigh is the default priority for content
  // compression. Setting this lower avoids compressing the content of the
  // scroll view.
  heightConstraint.priority = UILayoutPriorityDefaultHigh - 1;
  heightConstraint.active = YES;

  ChromeDirectionalEdgeInsets stackViewInsets =
      ChromeDirectionalEdgeInsetsMake(kAlertMarginTop, 0, 0, 0);
  AddSameConstraintsWithInsets(stackView, scrollView, stackViewInsets);

  if (self.title.length) {
    UILabel* titleLabel = [[UILabel alloc] init];
    titleLabel.numberOfLines = 0;
    titleLabel.font =
        [UIFont preferredFontForTextStyle:UIFontTextStyleHeadline];
    titleLabel.adjustsFontForContentSizeCategory = YES;
    titleLabel.textAlignment = NSTextAlignmentCenter;
    titleLabel.text = self.title;
    titleLabel.translatesAutoresizingMaskIntoConstraints = NO;
    [stackView addArrangedSubview:titleLabel];
    [stackView setCustomSpacing:kTitleInsetBottom afterView:titleLabel];

    ChromeDirectionalEdgeInsets titleInsets = ChromeDirectionalEdgeInsetsMake(
        0, kTitleInsetLeading, 0, kTitleInsetTrailing);
    AddSameConstraintsToSidesWithInsets(
        titleLabel, self.contentView,
        LayoutSides::kTrailing | LayoutSides::kLeading, titleInsets);
  }

  if (self.message.length) {
    UILabel* messageLabel = [[UILabel alloc] init];
    messageLabel.numberOfLines = 0;
    messageLabel.font =
        [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote];
    messageLabel.adjustsFontForContentSizeCategory = YES;
    messageLabel.textAlignment = NSTextAlignmentCenter;
    messageLabel.text = self.message;
    messageLabel.translatesAutoresizingMaskIntoConstraints = NO;
    [stackView addArrangedSubview:messageLabel];
    [stackView setCustomSpacing:kMessageInsetBottom afterView:messageLabel];

    ChromeDirectionalEdgeInsets messageInsets = ChromeDirectionalEdgeInsetsMake(
        0, kMessageInsetLeading, 0, kMessageInsetTrailing);
    AddSameConstraintsToSidesWithInsets(
        messageLabel, self.contentView,
        LayoutSides::kTrailing | LayoutSides::kLeading, messageInsets);
  }

  if (self.textFieldConfigurations.count) {
    // |stackHolder| has the background, border and round corners of the stacked
    // fields.
    UIView* stackHolder = [[UIView alloc] init];
    stackHolder.layer.cornerRadius = kTextFieldCornerRadius;
    stackHolder.layer.borderColor = UIColor.cr_separatorColor.CGColor;
#if defined(__IPHONE_13_0) && (__IPHONE_OS_VERSION_MAX_ALLOWED >= __IPHONE_13_0)
    if (@available(iOS 13, *)) {
      // Use performAsCurrentTraitCollection to get the correct CGColor for the
      // given dynamic color and current userInterfaceStyle.
      [self.traitCollection performAsCurrentTraitCollection:^{
        stackHolder.layer.borderColor = UIColor.cr_separatorColor.CGColor;
      }];
    }
#endif
    stackHolder.layer.borderWidth = 1.0 / [UIScreen mainScreen].scale;
    stackHolder.clipsToBounds = YES;
    stackHolder.backgroundColor = UIColor.cr_secondarySystemBackgroundColor;
    stackHolder.translatesAutoresizingMaskIntoConstraints = NO;
    self.textFieldStackHolder = stackHolder;

    // Updates the custom space before the text fields to account for their
    // inset.
    UIView* previousView = stackView.arrangedSubviews.lastObject;
    if (previousView) {
      CGFloat spaceBefore = [stackView customSpacingAfterView:previousView];
      [stackView setCustomSpacing:kTextfieldStackInsetTop + spaceBefore
                        afterView:previousView];
    } else {
      // There should always be a title or message.
      NOTREACHED() << "Presenting alert without a title or message.";
    }
    [stackView addArrangedSubview:stackHolder];

    ChromeDirectionalEdgeInsets stackHolderContentInsets =
        ChromeDirectionalEdgeInsetsMake(0, kTextfieldStackInsetLeading, 0,
                                        kTextfieldStackInsetTrailing);
    AddSameConstraintsToSidesWithInsets(
        stackHolder, self.contentView,
        LayoutSides::kTrailing | LayoutSides::kLeading,
        stackHolderContentInsets);

    UIStackView* fieldStack = [[UIStackView alloc] init];
    fieldStack.axis = UILayoutConstraintAxisVertical;
    fieldStack.translatesAutoresizingMaskIntoConstraints = NO;
    fieldStack.spacing = kTextfieldInset;
    fieldStack.alignment = UIStackViewAlignmentCenter;
    [stackHolder addSubview:fieldStack];
    ChromeDirectionalEdgeInsets fieldStackContentInsets =
        ChromeDirectionalEdgeInsetsMake(kTextfieldInset, 0.0, kTextfieldInset,
                                        0.0);
    AddSameConstraintsWithInsets(fieldStack, stackHolder,
                                 fieldStackContentInsets);

    NSMutableArray<UITextField*>* textFields = [[NSMutableArray alloc]
        initWithCapacity:self.textFieldConfigurations.count];
    for (TextFieldConfiguration* textFieldConfiguration in self
             .textFieldConfigurations) {
      if (textFieldConfiguration !=
          [self.textFieldConfigurations firstObject]) {
        UIView* hairline = [[UIView alloc] init];
        hairline.backgroundColor = UIColor.cr_separatorColor;
        hairline.translatesAutoresizingMaskIntoConstraints = NO;
        [fieldStack addArrangedSubview:hairline];
        CGFloat pixelHeight = 1.0 / [UIScreen mainScreen].scale;
        [hairline.heightAnchor constraintEqualToConstant:pixelHeight].active =
            YES;
        AddSameConstraintsToSides(
            fieldStack, hairline,
            LayoutSides::kTrailing | LayoutSides::kLeading);
      }
      UITextField* textField = [[UITextField alloc] init];
      textField.text = textFieldConfiguration.text;
      textField.placeholder = textFieldConfiguration.placeholder;
      textField.secureTextEntry = textFieldConfiguration.secureTextEntry;
      textField.accessibilityIdentifier =
          textFieldConfiguration.accessibilityIdentifier;
      textField.translatesAutoresizingMaskIntoConstraints = NO;
      textField.delegate = self;
      textField.font = [UIFont preferredFontForTextStyle:UIFontTextStyleBody];
      textField.adjustsFontForContentSizeCategory = YES;

      [fieldStack addArrangedSubview:textField];
      ChromeDirectionalEdgeInsets fieldInsets = ChromeDirectionalEdgeInsetsMake(
          0.0, kTextfieldInset, 0.0, kTextfieldInset);
      AddSameConstraintsToSidesWithInsets(
          textField, fieldStack, LayoutSides::kTrailing | LayoutSides::kLeading,
          fieldInsets);
      [textFields addObject:textField];
    }
    self.textFields = textFields;
  }

  UIView* lastArrangedView = stackView.arrangedSubviews.lastObject;
  if (lastArrangedView) {
    [stackView setCustomSpacing:kAlertActionsSpacing
                      afterView:lastArrangedView];
  }

  self.buttonAlertActionsDictionary = [[NSMutableDictionary alloc] init];
  for (AlertAction* action in self.actions) {
    UIView* hairline = [[UIView alloc] init];
    hairline.backgroundColor = UIColor.cr_separatorColor;
    hairline.translatesAutoresizingMaskIntoConstraints = NO;
    [stackView addArrangedSubview:hairline];

    GrayHighlightButton* button = [[GrayHighlightButton alloc] init];
    UIFont* font = nil;
    UIColor* textColor = nil;
    if (action.style == UIAlertActionStyleDefault) {
      font = [UIFont preferredFontForTextStyle:UIFontTextStyleBody];
      textColor = [UIColor colorNamed:kTintColor];
    } else if (action.style == UIAlertActionStyleCancel) {
      font = [UIFont preferredFontForTextStyle:UIFontTextStyleHeadline];
      textColor = [UIColor colorNamed:kTintColor];
    } else {  // Style is UIAlertActionStyleDestructive
      font = [UIFont preferredFontForTextStyle:UIFontTextStyleBody];
      textColor = [UIColor colorNamed:kDestructiveTintColor];
    }
    button.titleLabel.font = font;
    button.titleLabel.adjustsFontForContentSizeCategory = YES;

    [button setTitleColor:textColor forState:UIControlStateNormal];
    [button setTitle:action.title forState:UIControlStateNormal];

    button.contentHorizontalAlignment =
        UIControlContentHorizontalAlignmentCenter;
    [button addTarget:self
                  action:@selector(didSelectActionForButton:)
        forControlEvents:UIControlEventTouchUpInside];
    button.translatesAutoresizingMaskIntoConstraints = NO;
    button.contentEdgeInsets =
        UIEdgeInsetsMake(kButtonInsetTop, kButtonInsetLeading,
                         kButtonInsetBottom, kButtonInsetTrailing);
    [stackView addArrangedSubview:button];

    CGFloat pixelHeight = 1.0 / [UIScreen mainScreen].scale;
    [hairline.heightAnchor constraintEqualToConstant:pixelHeight].active = YES;
    AddSameConstraintsToSides(hairline, button,
                              (LayoutSides::kTrailing | LayoutSides::kLeading));

    AddSameConstraintsToSides(button, self.contentView,
                              LayoutSides::kTrailing | LayoutSides::kLeading);

    button.tag = action.uniqueIdentifier;
    self.buttonAlertActionsDictionary[@(action.uniqueIdentifier)] = action;
  }

  [[NSNotificationCenter defaultCenter]
      addObserver:self
         selector:@selector(handleKeyboardWillShow:)
             name:UIKeyboardWillShowNotification
           object:nil];

  [[NSNotificationCenter defaultCenter]
      addObserver:self
         selector:@selector(handleKeyboardWillHide:)
             name:UIKeyboardWillHideNotification
           object:nil];
}

#pragma mark - Getters

- (NSArray<NSString*>*)textFieldResults {
  if (!self.textFields) {
    return nil;
  }
  NSMutableArray<NSString*>* results =
      [[NSMutableArray alloc] initWithCapacity:self.textFields.count];
  for (UITextField* textField in self.textFields) {
    [results addObject:textField.text];
  }
  return results;
}

#pragma mark - UIGestureRecognizerDelegate

- (BOOL)gestureRecognizer:(UIGestureRecognizer*)gestureRecognizer
       shouldReceiveTouch:(UITouch*)touch {
  if (self.tapRecognizer != gestureRecognizer) {
    return YES;
  }
  CGPoint locationInContentView = [touch locationInView:self.contentView];
  return !CGRectContainsPoint(self.contentView.bounds, locationInContentView);
}

#pragma mark - UITextFieldDelegate

- (void)textFieldDidBeginEditing:(UITextField*)textField {
  self.lastFocusedTextField = textField;
}

- (BOOL)textFieldShouldReturn:(UITextField*)textField {
  NSUInteger index = [self.textFields indexOfObject:textField];
  if (index + 1 < self.textFields.count) {
    [self.textFields[index + 1] becomeFirstResponder];
  } else {
    [textField resignFirstResponder];
  }
  return NO;
}

#pragma mark - Private

- (void)handleKeyboardWillShow:(NSNotification*)notification {
  CGRect keyboardFrame =
      [notification.userInfo[UIKeyboardFrameEndUserInfoKey] CGRectValue];
  CGRect viewFrameInWindow = [self.view convertRect:self.view.bounds
                                             toView:nil];
  CGRect intersectedFrame =
      CGRectIntersection(keyboardFrame, viewFrameInWindow);

  CGFloat additionalBottomInset =
      intersectedFrame.size.height - self.view.safeAreaInsets.bottom;

  if (additionalBottomInset > 0) {
    self.additionalSafeAreaInsets =
        UIEdgeInsetsMake(0, 0, additionalBottomInset, 0);
    [self animateLayoutFromKeyboardNotification:notification];
  }

  self.tapRecognizer.enabled = YES;
  self.swipeRecognizer.enabled = YES;
}

- (void)handleKeyboardWillHide:(NSNotification*)notification {
  self.additionalSafeAreaInsets = UIEdgeInsetsZero;
  [self animateLayoutFromKeyboardNotification:notification];

  self.tapRecognizer.enabled = NO;
  self.swipeRecognizer.enabled = NO;
}

- (void)animateLayoutFromKeyboardNotification:(NSNotification*)notification {
  double duration =
      [notification.userInfo[UIKeyboardAnimationDurationUserInfoKey]
          doubleValue];
  UIViewAnimationCurve animationCurve = static_cast<UIViewAnimationCurve>(
      [notification.userInfo[UIKeyboardAnimationCurveUserInfoKey]
          integerValue]);
  UIViewAnimationOptions options = animationCurve
                                   << kUIViewAnimationCurveToOptionsShift;

  [UIView animateWithDuration:duration
                        delay:0
                      options:options
                   animations:^{
                     [self.view layoutIfNeeded];
                   }
                   completion:nil];
}

- (void)didSelectActionForButton:(UIButton*)button {
  AlertAction* action = self.buttonAlertActionsDictionary[@(button.tag)];
  if (action.handler) {
    action.handler(action);
  }
}

- (void)dismissKeyboard {
  [self.lastFocusedTextField resignFirstResponder];
}

@end
