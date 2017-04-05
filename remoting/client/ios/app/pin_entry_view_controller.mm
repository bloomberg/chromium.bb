// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

#import "remoting/client/ios/app/pin_entry_view_controller.h"

#import "ios/third_party/material_components_ios/src/components/Buttons/src/MaterialButtons.h"
#import "ios/third_party/material_components_ios/src/components/NavigationBar/src/MaterialNavigationBar.h"

static const CGFloat kHostCardIconSize = 45.f;

@interface PinEntryViewController () {
  PinEntryModalCallback _callback;
  MDCNavigationBar* _navBar;
}
@end

@implementation PinEntryViewController

- (id)initWithCallback:(PinEntryModalCallback)callback {
  self = [super init];
  if (self) {
    _callback = callback;
  }
  return self;
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];
  self.view.backgroundColor = [UIColor whiteColor];

  // TODO(nicholss): Localize this file.
  self.navigationItem.rightBarButtonItem =
      [[UIBarButtonItem alloc] initWithTitle:@"CANCEL"
                                       style:UIBarButtonItemStylePlain
                                      target:self
                                      action:@selector(didTapCancel:)];

  _navBar = [[MDCNavigationBar alloc] initWithFrame:CGRectZero];
  [_navBar observeNavigationItem:self.navigationItem];

  [_navBar setBackgroundColor:[UIColor blackColor]];
  MDCNavigationBarTextColorAccessibilityMutator* mutator =
      [[MDCNavigationBarTextColorAccessibilityMutator alloc] init];
  [mutator mutate:_navBar];

  [self.view addSubview:_navBar];
  _navBar.translatesAutoresizingMaskIntoConstraints = NO;

  UIView* entryView = [[UIView alloc] initWithFrame:CGRectZero];

  [self.view addSubview:entryView];
  entryView.translatesAutoresizingMaskIntoConstraints = NO;

  UIImageView* imageView = [[UIImageView alloc]
      initWithFrame:CGRectMake(0, 0, kHostCardIconSize, kHostCardIconSize)];
  imageView.contentMode = UIViewContentModeCenter;
  imageView.alpha = 0.87f;
  imageView.backgroundColor = UIColor.lightGrayColor;
  imageView.layer.cornerRadius = kHostCardIconSize / 2.f;
  imageView.layer.masksToBounds = YES;
  imageView.translatesAutoresizingMaskIntoConstraints = NO;
  [self.view addSubview:imageView];

  UILabel* titleLabel = [[UILabel alloc] initWithFrame:CGRectZero];
  titleLabel.textColor = [UIColor colorWithWhite:0 alpha:0.87f];
  titleLabel.text = @"Host XXX";
  titleLabel.translatesAutoresizingMaskIntoConstraints = NO;
  [self.view addSubview:titleLabel];
  [titleLabel sizeToFit];

  MDCRaisedButton* pinButton = [[MDCRaisedButton alloc] init];
  [pinButton setTitle:@"->" forState:UIControlStateNormal];
  [pinButton sizeToFit];
  [pinButton addTarget:self
                action:@selector(didTapPinEntry:)
      forControlEvents:UIControlEventTouchUpInside];
  pinButton.translatesAutoresizingMaskIntoConstraints = NO;
  [entryView addSubview:pinButton];

  UITextField* pinEntry =
      [[UITextField alloc] initWithFrame:CGRectMake(0, 0, 100, 10)];
  pinEntry.textAlignment = NSTextAlignmentCenter;
  pinEntry.secureTextEntry = YES;
  pinEntry.autoresizingMask = UIViewAutoresizingFlexibleWidth;
  pinEntry.keyboardType = UIKeyboardTypeNumberPad;
  pinEntry.attributedPlaceholder =
      [[NSAttributedString alloc] initWithString:@"Enter PIN"];
  [entryView addSubview:pinEntry];
  pinEntry.translatesAutoresizingMaskIntoConstraints = NO;
  [pinEntry sizeToFit];

  NSDictionary* views = @{
    @"entryView" : entryView,
    @"navBar" : _navBar,
    @"imageView" : imageView,
    @"titleLabel" : titleLabel,
    @"pinEntry" : pinEntry,
    @"pinButton" : pinButton
  };

  NSDictionary* metrics = @{
    @"imageEdge" : @150.0,
    @"padding" : @15.0,
    @"imageSize" : @45.0,
    @"buttonSize" : @70.0
  };

  [self.view addConstraints:[NSLayoutConstraint
                                constraintsWithVisualFormat:@"|[navBar]|"
                                                    options:0
                                                    metrics:metrics
                                                      views:views]];
  [self.view addConstraints:
                 [NSLayoutConstraint
                     constraintsWithVisualFormat:@"|-[imageView(==imageSize)]-|"
                                         options:0
                                         metrics:metrics
                                           views:views]];

  [self.view addConstraints:
                 [NSLayoutConstraint
                     constraintsWithVisualFormat:@"|-[titleLabel]-|"
                                         options:NSLayoutFormatAlignAllCenterX
                                         metrics:metrics
                                           views:views]];
  [self.view addConstraints:[NSLayoutConstraint
                                constraintsWithVisualFormat:@"|[entryView]|"
                                                    options:0
                                                    metrics:metrics
                                                      views:views]];
  NSString* vViewConstraint =
      @"V:|[navBar]-[imageView(==imageSize)]-[titleLabel]-[entryView]|";
  [self.view addConstraints:[NSLayoutConstraint
                                constraintsWithVisualFormat:vViewConstraint
                                                    options:0
                                                    metrics:metrics
                                                      views:views]];

  [entryView addConstraints:
                 [NSLayoutConstraint
                     constraintsWithVisualFormat:
                         @"|-[pinEntry]-[pinButton(==buttonSize)]-|"
                                         options:NSLayoutFormatAlignAllCenterY
                                         metrics:metrics
                                           views:views]];
  [entryView addConstraints:
                 [NSLayoutConstraint
                     constraintsWithVisualFormat:@"V:|->=padding-[pinButton]"
                                         options:0
                                         metrics:metrics
                                           views:views]];
  [entryView
      addConstraints:[NSLayoutConstraint
                         constraintsWithVisualFormat:@"V:|->=padding-[pinEntry]"
                                             options:0
                                             metrics:metrics
                                               views:views]];
  [entryView layoutIfNeeded];
}

- (void)viewWillAppear:(BOOL)animated {
  [super viewWillAppear:animated];

  [self.navigationController setNavigationBarHidden:YES animated:animated];
}

#pragma mark - Private

- (void)didTapPinEntry:(id)sender {
  NSLog(@"%@ was tapped.", NSStringFromClass([sender class]));
}

- (void)didTapCancel:(id)sender {
  NSLog(@"%@ was tapped.", NSStringFromClass([sender class]));
}

@end
